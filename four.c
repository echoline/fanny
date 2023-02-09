#include <floatfann.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>

int
main(int argc, char **argv)
{
	if (argc < 4)
		return -1;
	struct fann *ann = fann_create_from_file(argv[1]);
	if (ann == NULL)
		return -2;
	fann_set_activation_function_hidden(ann, FANN_LINEAR_PIECE_LEAKY);
	fann_set_activation_function_output(ann, FANN_SIGMOID);
	int fd, r, offset, x, y;
	unsigned char data[3660];
	fann_type *input = calloc(sizeof(fann_type), ann->num_input);
	fann_type *output = calloc(sizeof(fann_type), ann->num_output);
	fann_type *results;
	double err, left, right, lastleft, lastright, tilt, tiltx, tilty, tiltz;
	char running = 1, readinput;
	struct pollfd fds[1];

	fds[0].fd = 0;
	fds[0].events = POLLIN | POLLHUP;

	lastleft = lastright = 0.0;

	while(running) {
		fd = open(argv[2], O_RDONLY);
		if (fd < 0)
			return -3;
		offset = 0;
		while((r = read(fd, &data[offset], sizeof(data) - offset)) > 0)
			offset += r;
		if (r < 0)
			return -4;
		close(fd);

		memmove(&input[3600], &input[3000], 4200 * sizeof(fann_type));
		for (offset = 0; offset < 600; offset++)
			input[3000 + offset] = output[offset] * 255.0;
		offset = 0;
		for (y = 0; y < 30; y++)
			for (x = 20; x < 120; x++, offset++)
				input[offset] = data[y * 120 + x];
		offset = 0;
		for (y = 0; y < 30; y++)
			for (x = 0; x < 20; x++, offset++)
				output[offset] = data[y * 120 + x] / 255.0;

		fd = open(argv[3], O_WRONLY);
		if (fd < 0)
			return -5;
		write(fd, "0", 1);
		close(fd);
		fd = open(argv[3], O_RDONLY);
		if (fd < 0)
			return -6;
		offset = 0;
		while((r = read(fd, &data[offset], sizeof(data) - offset)) > 0)
			offset += r;
		if (r < 0)
			return -7;
		close(fd);
		data[offset] = '\0';
		sscanf(data, "%lf %lf %lf %lf\n", &tilt, &tiltx, &tilty, &tiltz);
		memmove(&input[7803], &input[7800], 21 * sizeof(fann_type));
		input[7800] = tiltx;
		input[7801] = tilty;
		input[7802] = tiltz;

		offset = 0;
		readinput = 0;
		while(poll(fds, 1, 0) > 0) {
			if (fds[0].revents & POLLHUP)
				return 0;
			if (fds[0].revents & POLLIN) {
				r = read(fds[0].fd, &data[offset], sizeof(data) - offset);
				if (r == 0)
					continue;
				if (r < 0)
					return -8;
				offset += r;
				data[offset] = '\0';
				readinput = 1;
			}
		}
		if (readinput == 1) {
			if (data[0] == 's')
				fann_save(ann, argv[1]);
			else {
				sscanf(data, "%lf %lf", &left, &right);
				left /= 2.0;
				right /= 2.0;
				if (left > 100.0)
					left = 100.0;
				else if (left < -50.0)
					left = -50.0;
				if (right > 100.0)
					right = 100.0;
				else if (right < -50.0)
					right = -50.0;
				if (left > 0) for (offset = 0; offset < 100; offset++) {
					if (offset < left)
						output[600 + offset] = 1.0;
					else
						output[600 + offset] = 0.0;
				} else for (offset = 0; offset < 100; offset++)
					output[600 + offset] = 0.0;
				if (right > 0) for (offset = 0; offset < 100; offset++) {
					if (offset < right)
						output[750 + offset] = 1.0;
					else
						output[750 + offset] = 0.0;
				} else for (offset = 0; offset < 100; offset++)
					output[750 + offset] = 0.0;
				if (left < 0) for (offset = 0; offset > -50; offset--) {
					if (offset < left)
						output[749 + offset] = 0.0;
					else
						output[749 + offset] = 1.0;
				} else for (offset = 0; offset > -50; offset--)
					output[749 + offset] = 0.0;
				if (right < 0) for (offset = 0; offset > -50; offset--) {
					if (offset < right)
						output[899 + offset] = 0.0;
					else
						output[899 + offset] = 1.0;
				} else for (offset = 0; offset > -50; offset--)
					output[899 + offset] = 0.0;
			}
		}

		fann_train(ann, input, output);

		err = 0;
		for (offset = 0; offset < 600; offset++)
			err += abs(output[offset] - ann->output[offset]);
		err /= 600.0;
		for (offset = 0; offset < 300; offset++)
			output[600 + offset] = 1.0 - err;
		memmove(&input[7048], &input[6924], (ann->num_input - 7048) * sizeof(fann_type));
		for (offset = 900; offset < ann->num_output; offset++)
			input[6924 + offset] = output[offset] = ann->output[offset];

		left = right = 0.0;
		for (offset = 0; offset < 100; offset++)
			left += ann->output[600 + offset];
		for (offset = 100; offset < 150; offset++)
			left -= ann->output[600 + offset];
		left *= 2;
		for (offset = 150; offset < 250; offset++)
			right += ann->output[600 + offset];
		for (offset = 250; offset < 300; offset++)
			right -= ann->output[600 + offset];
		right *= 2;

		if (left < (lastleft - 20.0))
			left = lastleft - 20.0;
		else if (left > (lastleft + 20.0))
			left = lastleft + 20.0;
		if (right < (lastright - 20.0))
			right = lastright - 20.0;
		else if (right > (lastright + 20.0))
			right = lastright + 20.0;

		lastleft = left;
		lastright = right;

		fprintf(stdout, "%d %d\n", (int)left, (int)right);
		fprintf(stderr, "surprise: %0.10lf left: %0.5lf right: %0.5lf tiltx: %0.5lf tilty: %0.5lf tiltz: %0.5lf\n", err, left, right, tiltx, tilty, tiltz);
	}

	return 0;
}
