#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <jpeglib.h>
#include <math.h>
#include <parallel_fann.h>
#include <signal.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#define EYES "/home/eli/kinect/inputs.bit"
#define TILT "/home/eli/kinect/tilt"
#define BRAIN "/mnt/sdc1/anns/hEather.fann"

struct fann *ann = NULL;
unsigned char running = 1;
unsigned char gotsigchld = 1;
char movechar = 0;

void
ctrlc(int sig) {
	if (sig == SIGINT)
		running = 0;
}

void
sigchld(int sig) {
	if (sig == SIGCHLD)
		gotsigchld++;
}

int
main(int argc, char **argv) {
	int s, x, y, o, n, m;
	int rc;
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
	int row_stride, width, height, pixel_size;
	int hwidth, hheight;
	unsigned char *buf;
	unsigned char *jbuf = malloc(640*4 + 1);
	unsigned char *tmp;
	unsigned char *depths[] = {NULL, NULL, NULL, NULL, NULL, NULL, };
	unsigned char *edgess[] = {NULL, NULL, NULL, NULL, NULL, NULL, };
	unsigned char *depthmiddles[] = {NULL, NULL, NULL, NULL, NULL, NULL, };
	unsigned char *edgesmiddles[] = {NULL, NULL, NULL, NULL, NULL, NULL, };
	unsigned char *bbuf = malloc(640*480*4);
	fann_type *results;
	fann_type deptherror, edgeserror;
	fann_type lowestdeptherror, lowestedgeserror;
	int depthwinner, edgeswinner;
	struct fann_neuron *neuron;
	FILE *file, *tiltfile = NULL;
	int dx, dy;
	int counter;
	int lowest;
	int highest;
	int lastlowest;
	int lasthighest;
	fann_type fidget, surprise, avg, sum;
	int tilt = 0;
	struct pollfd fds[1];
	struct stat statbuf;
	pid_t pid;
	int jpegpipe[2];
	unsigned char *depth;
	unsigned char *edges;
	unsigned char *bjpeg;
	fann_type *input;
	fann_type *output;
	fann_type *motors;
	int *lasts;
	fann_type *votes;
	char *strbuf;
	int eyesfd;

	eyesfd = open(EYES, O_RDONLY);
	if (eyesfd < 0)
		return -1;

	ann = fann_create_from_file(BRAIN);
	if (ann == NULL)
		return -1;
	fann_set_activation_function_hidden(ann, FANN_LINEAR_PIECE_LEAKY);
	fann_set_activation_function_output(ann, FANN_SIGMOID);

	depth = malloc(640*480*4);
	edges = malloc(640*480*4);
	bjpeg = malloc(640*480*4);
	input = malloc(60000 * sizeof(fann_type));
	output = malloc(1000 * sizeof(fann_type));
	motors = malloc(100 * sizeof(fann_type));
	lasts = malloc(100 * sizeof(int));
	votes = malloc(5 * sizeof(fann_type));
	strbuf = malloc(32);

	memset(input, 0, 60000*sizeof(fann_type));
	memset(output, 0, 1000*sizeof(fann_type));
	for (n = 0; n < 100; n++) {
		lasts[n] = 600;
	}

//	signal(SIGINT, &ctrlc);
	signal(SIGCHLD, &sigchld);

	fds[0].fd = 0;
	fds[0].events = POLLIN | POLLHUP;

	while (running != 0) {
		o = 3660;
		do {
			rc = read(eyesfd, &bbuf[3660 - o], o);
			if (rc <= 0) {
				perror("read: eyesfd");
				break;
			}
			o -= rc;
		} while(o > 0);

		if (o > 0) fprintf(stderr, "partial image read: %d bytes left\n", o);

		for (n = 0; n < 6; n++) {
			depthmiddles[n] = malloc(20*15);
			edgesmiddles[n] = malloc(20*15);
			for (y = 0; y < 15; y++) for (x = 0; x < 20; x++) {
				depthmiddles[n][y*20+x] = bbuf[y*20+x+n*20+60];
				edgesmiddles[n][y*20+x] = bbuf[y*20+x+n*20+60+1800];
			}
		}

		results = fann_run(ann, input);
		lowestdeptherror = 300;
		lowestedgeserror = 300;
		for (n = 0; n < 6; n++) {
			deptherror = 0;
			edgeserror = 0;
			for (x = 0; x < 300; x++) {
				deptherror += pow(depthmiddles[n][x]/255.0 - results[x], 2);
				edgeserror += pow(edgesmiddles[n][x]/255.0 - results[x+300], 2);
			}
			if (deptherror < lowestdeptherror) {
				lowestdeptherror = deptherror;
				depthwinner = n;
			}
			if (edgeserror < lowestedgeserror) {
				lowestedgeserror = edgeserror;
				edgeswinner = n;
			}
		}
//		depthwinner = edgeswinner = 5;
		memcpy(&output[700], &results[700], 300*sizeof(fann_type));
		for (n = 0; n < 300; n++) {
			output[n] = depthmiddles[depthwinner][n] / 255.0;
			output[300+n] = edgesmiddles[edgeswinner][n] / 255.0;
		}
		for (y = 0; y < 15; y++) for (x = 0; x < 20; x++) {
			bbuf[y*width*4+(x+20*depthwinner)*4] |= (unsigned char)(results[(y*20+x)] * 255.0);
			bbuf[y*width*4+(x+20*edgeswinner)*4] |= (unsigned char)(results[(y*20+x)+300] * 255.0);
		}

		memset(votes, 0, 5 * sizeof(int));
		memcpy(motors, &results[600], 100 * sizeof(fann_type));

		strbuf[0] = '\0';
		while (poll(fds, 1, 0) > 0) {
			strbuf[0] = '\0';
			if (fds[0].revents & POLLHUP)
				goto END;
			if (fds[0].revents & POLLIN) {
				n = read(fds[0].fd, strbuf, 16);
				if (n <= 0)
					goto END;
			}
		}
		if (movechar != 0)
			strbuf[0] = movechar;
		switch (strbuf[0]) {
			case 'w':
				for (n = 0; n < 100; n++)
					motors[n] *= 0.05;
				for (n = 0; n < 20; n++)
					motors[rand() % 20] = 1.0;
				break;
			case 'a':
				for (n = 0; n < 100; n++)
					motors[n] *= 0.05;
				for (n = 20; n < 40; n++)
					motors[20 + rand() % 20] = 1.0;
				break;
			case 'd':
				for (n = 0; n < 100; n++)
					motors[n] *= 0.05;
				for (n = 40; n < 60; n++)
					motors[40 + rand() % 20] = 1.0;
				break;
			case 'r':
				for (n = 0; n < 100; n++)
					motors[n] *= 0.05;
				for (n = 60; n < 80; n++)
					motors[60 + rand() % 20] = 1.0;
				break;
			case 'f':
				for (n = 0; n < 100; n++)
					motors[n] *= 0.05;
				for (n = 80; n < 100; n++)
					motors[80 + rand() % 20] = 1.0;
				break;
			case 's':
				fann_save(ann, BRAIN);
				break;
			default:
				break;
		}

		sum = 0;
		for (n = 0; n < 100; n++) {
			votes[n/20] += motors[n];
			sum += motors[n];
		}
		avg = sum / 100.0;
		sum = 0;
		for (n = 0; n < 100; n++)
			sum += motors[n] * motors[n];
		sum /= 100.0;
		sum -= avg * avg;

		if (votes[0] > (votes[1] + votes[2] + votes[3] + votes[4]))
			printf("w\n");
		else if (votes[1] > (votes[0] + votes[2] + votes[3] + votes[4]))
			printf("a\n");
		else if (votes[2] > (votes[0] + votes[1] + votes[3] + votes[4]))
			printf("d\n");
		else if (votes[3] > (votes[0] + votes[1] + votes[2] + votes[4])) {
			if (tilt < 30)
				tilt += 3;
		}
		else if (votes[4] > (votes[0] + votes[1] + votes[2] + votes[3])) {
			if (tilt > -30)
				tilt -= 3;
		}
		if (tiltfile != NULL) {
			if (fprintf(tiltfile, "%d\n", tilt) < 0) {
				fclose(tiltfile);
				tiltfile = NULL;
			} else {
				memmove(&input[59006], &input[59001], 990);
				fscanf(tiltfile, "%d %f %f %f\n", &tilt, &input[59003], &input[59004], &input[59005]);
			}
		}
		else {
			tiltfile = fopen(TILT, "a+");
			fprintf(stderr, "reopening tiltfile\n");
		}
		fflush(stdout);
		movechar = 0;

		// i don't get why this works
		for (n = 0; n < 100; n++) {
			motors[n] -= (motors[n] - avg) - (0.2 - avg) * fidget;
//			motors[n] -= (motors[n] - avg - 0.2) * fidget * motors[n] + 0.00001;
		}
		memcpy(&output[600], motors, 100 * sizeof(fann_type));

//		struct fann_train_data *fanndata = fann_create_train_array(1, 60000, input, 1000, output);
//		fann_train_epoch(ann, fanndata);
//		fann_destroy_train(fanndata);
		fann_train(ann, input, output);

		memmove(&lasts[1], lasts, 9*sizeof(int));
		lasts[0] = fann_get_bit_fail(ann);
		lowest = 600;
		highest = 0;
		for (n = 0; n < 10; n++) {
			if (lasts[n] < lowest)
				lowest = lasts[n];
			if (lasts[n] > highest)
				highest = lasts[n];
		}
		if (highest > lasthighest)
			counter = 0;
		else
			counter++;
		lastlowest = lowest;
		lasthighest = highest;
		fann_reset_MSE(ann);
		fidget = counter / 10.0;
		if (fidget > 1.0)
			fidget = 1.0;
		surprise = (lasthighest + lastlowest) / 100.0;
		fprintf(stderr, "fidget: %2f\tsurprise: %10f mvar: %10f mavg: %10f\n", fidget, surprise, sum, avg);

		memmove(&input[3600], input, 32400*sizeof(fann_type));
		for (y = 0; y < 15; y++) for(x = 0; x < 120; x++) {
			input[y*240+x*2] = bbuf[y*width*4+x*4+1] / 255.0;
			input[y*240+x*2+1] = bbuf[y*width*4+x*4+2] / 255.0;
		}
		memmove(&input[36000+2000], &input[36000], 21000*sizeof(fann_type));
		memcpy(&input[36000], results, 1000*sizeof(fann_type));
		neuron = ann->first_layer[2].first_neuron;
		for (n = 0; n < 1000; n++)
			input[36000+1000+n] = neuron[n].value;
		input[59001] = fidget;
		input[59002] = surprise;
	}

END:
	fann_save(ann, BRAIN);
	return 0;
}
