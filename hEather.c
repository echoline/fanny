#include <X11/Xlib.h>
#include <X11/IntrinsicP.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <jpeglib.h>
#include <math.h>
#include <fann.h>
#include <signal.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#define EYES "/mnt/kinect/extra.jpg"
#define TILT "/mnt/kinect/tilt"
#define BRAIN "/mnt/sdc1/anns/hEather.fann"
#define STREAM "/home/eli/hub/mjpeg"
#define LOCK "/tmp/streamlock"

struct fann *ann = NULL;
unsigned char running = 1;
unsigned char gotsigchld = 0;

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

unsigned char*
halfscalealloc(int width, int height, unsigned char *data) {
	int length = width * height;
	int newlength = length / 4;
	unsigned char *newdata;
	int w = width / 2, h = height / 2;
	int x, y;

	if (width & 1 || height & 1) {
		fprintf(stderr, "image dimensions must be even: %dx%d\n", width, height);
		return NULL;
	}

	newdata = malloc(newlength);
	if (newdata == NULL)
		return NULL;

	for (x = 0; x < w; x++) for(y = 0; y < h; y++) {
		newdata[y*w+x] = (data[(y*2)*width+(x*2)] +
				data[(y*2+1)*width+(x*2+1)] +
				data[(y*2)*width+(x*2+1)] +
				data[(y*2+1)*width+(x*2)]) / 4;
	}

	return newdata;
}

unsigned char*
doublescalealloc(int width, int height, unsigned char *data) {
	int length = width * height;
	int newlength = length * 4;
	unsigned char *newdata;
	int x, y;

	newdata = malloc(newlength);

	for (x = 0; x < width; x++) for(y = 0; y < height; y++) {
		newdata[y*2*width+(x*2)] = data[y*width+x];
		newdata[(y+1)*2*width+(x*2)] = data[y*width+x];
		newdata[(y+1)*2*width+(x*2+1)] = data[y*width+x];
		newdata[y*2*width+(x*2+1)] = data[y*width+x];
	}

	return newdata;
}

unsigned char*
middleextract(int width, int height, unsigned char *data) {
	unsigned char *ret;
	int hwidth, hheight, x, y, i, j, m, n;
	if (width < 20 || height < 15)
		return NULL;
	ret = malloc(20 * 15);
	if (height == 15 && width == 20) {
		memcpy(ret, data, width * height);
	} else {
		hwidth = width / 2;
		hheight = height / 2;
		x = hwidth - 10;
		m = x + 20;
		y = hheight - 7;
		n = y + 15;
		if (height & 1 && width & 1) {
			fprintf(stderr, "unimplemented\n");
			free(ret);
			return NULL;
		} else if (height & 1) {
			for (i = y; i < n; i++) for(j = x; j < m; j++)
				ret[(i-y)*20+(j-x)] = data[i*width+j];
		} else if (width & 1) {
			fprintf(stderr, "unimplemented\n");
			free(ret);
			return NULL;
		} else {
			y--; n--;
			for (i = y; i < n; i++) for(j = x; j < m; j++)
				ret[(i-y)*20+(j-x)] = (data[i*width+j]+data[(i+1)*width+j])>>1;
		}
	}
	return ret;
}

size_t
compressjpg(int w, int h, unsigned char *in, unsigned char *out)
{
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	JSAMPROW row_pointer[1];
	unsigned char *buf;
	size_t buflen = 0;

	cinfo.err = jpeg_std_error( &jerr );
	jpeg_create_compress(&cinfo);

	cinfo.image_width = w;
	cinfo.image_height = h;
	cinfo.input_components = 4;
	cinfo.in_color_space = JCS_EXT_BGRX;

	jpeg_mem_dest( &cinfo, &buf, &buflen );
	jpeg_set_defaults( &cinfo );

	jpeg_start_compress( &cinfo, TRUE );
	while( cinfo.next_scanline < cinfo.image_height ) {
		row_pointer[0] = &in[ cinfo.next_scanline * cinfo.image_width * cinfo.input_components];

		jpeg_write_scanlines( &cinfo, row_pointer, 1 );
	}

	jpeg_finish_compress( &cinfo );

	memcpy(out, buf, buflen);

	jpeg_destroy_compress( &cinfo );
	free(buf);

	return buflen;
}

int
main(int argc, char **argv) {
	Display *d;
	Window w;
	XEvent e;
	int s, x, y, o, n, m;
	GC gc;
	XImage *i;
	int rc;
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
	int row_stride, width, height, pixel_size;
	int hwidth, hheight;
	unsigned char *buf;
	unsigned char *jbuf;
	unsigned char *tmp;
	unsigned char *bbuf = malloc(640*480*4);
	unsigned char *depth = malloc(640*480*4);
	unsigned char *edges = malloc(640*480*4);
	unsigned char *bjpeg = malloc(640*480*4);
	unsigned char *depths[] = {NULL, NULL, NULL, NULL, NULL, NULL, };
	unsigned char *edgess[] = {NULL, NULL, NULL, NULL, NULL, NULL, };
	unsigned char *depthmiddles[] = {NULL, NULL, NULL, NULL, NULL, NULL, };
	unsigned char *edgesmiddles[] = {NULL, NULL, NULL, NULL, NULL, NULL, };
	fann_type *input = malloc(60000 * sizeof(fann_type));
	fann_type *output = malloc(1000 * sizeof(fann_type));
	fann_type *motors = malloc(100 * sizeof(fann_type));
	fann_type *results;
	fann_type deptherror, edgeserror;
	fann_type lowestdeptherror, lowestedgeserror;
	int depthwinner, edgeswinner;
	struct fann_neuron *neuron;
	FILE *file, *tiltfile;
	int dx, dy;
	int counter;
	int lowest;
	int highest;
	int *lasts = malloc(100 * sizeof(int));
	int lastlowest;
	int lasthighest;
	fann_type fidget, surprise, avg, sum;
	fann_type *votes = malloc(5 * sizeof(fann_type));
	int tilt = 0;
	struct pollfd fds[1];
	char *strbuf = malloc(32);
	struct stat statbuf;

	ann = fann_create_from_file(BRAIN);
	if (ann == NULL)
		return -1;
	fann_set_activation_function_hidden(ann, FANN_LINEAR_PIECE_LEAKY);
	memset(input, 0, 60000*sizeof(fann_type));
	memset(output, 0, 1000*sizeof(fann_type));
	for (n = 0; n < 100; n++) {
		lasts[n] = 600;
	}

//	signal(SIGINT, &ctrlc);
	signal(SIGCHLD, &sigchld);

	file = fopen(EYES, "rb");
	if (file == NULL)
		return -1;

	tiltfile = fopen(TILT, "a+");

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);
	jpeg_stdio_src(&cinfo, file);

	rc = jpeg_read_header(&cinfo, FALSE);
	if (rc != 1) {
		jpeg_destroy_decompress(&cinfo);
		fprintf(stderr, "error: jpeg_read_header\n");
		return -1;
	}

	jpeg_start_decompress(&cinfo);
	width = cinfo.output_width;
	height = cinfo.output_height;
	pixel_size = cinfo.output_components;
	row_stride = width * pixel_size;

	jbuf = malloc(row_stride + 1);

	while (cinfo.output_scanline < cinfo.output_height)
		jpeg_read_scanlines(&cinfo, &jbuf, 1);

	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);
	fclose(file);

	d = XOpenDisplay(NULL);
	if (d == NULL) {
		fprintf(stderr, "Cannot open display\n");
		return -1;
	}
 
	s = DefaultScreen(d);
	w = XCreateSimpleWindow(d, RootWindow(d, s), 0, 0, width, height, 1,
					BlackPixel(d, s), BlackPixel(d, s));
	XSelectInput(d, w, ExposureMask | KeyPressMask);
	XMapWindow(d, w);
	gc = DefaultGC (d, s);

	fds[0].fd = 0;
	fds[0].events = POLLIN | POLLHUP;

	while (running != 0) {
		file = fopen(EYES, "rb");
		if (file == NULL)
			return -1;

		cinfo.err = jpeg_std_error(&jerr);
		jpeg_create_decompress(&cinfo);
		jpeg_stdio_src(&cinfo, file);

		rc = jpeg_read_header(&cinfo, FALSE);
		if (rc != 1) {
			jpeg_destroy_decompress(&cinfo);
			fprintf(stderr, "error: jpeg_read_header\n");
			continue;
		}

		jpeg_start_decompress(&cinfo);
		width = cinfo.output_width;
		height = cinfo.output_height;
		hwidth = width / 2;
		hheight = height / 2;
		pixel_size = cinfo.output_components;
		row_stride = width * pixel_size;

//		fprintf(stderr, "%d %d %d %d\n", width, height, pixel_size, row_stride);

		if (pixel_size == 3) {
			while (cinfo.output_scanline < cinfo.output_height) {
				y = cinfo.output_scanline;
				jpeg_read_scanlines(&cinfo, &jbuf, 1);
				for (x = 0; x < width; x++) {
					o = x + y*width;
					bbuf[o*4] = 0;
					edges[o] = bbuf[o*4+1] = jbuf[x*3];
					depth[o] = bbuf[o*4+2] = jbuf[x*3+2];
				}
			}
		} else {
			return -1;
		}

		jpeg_finish_decompress(&cinfo);
		jpeg_destroy_decompress(&cinfo);

		fclose(file);

		if (edgesmiddles[0] != NULL) free(edgesmiddles[0]);
		edgesmiddles[0] = middleextract(width, height, edges);
		if (depthmiddles[0] != NULL) free(depthmiddles[0]);
		depthmiddles[0] = middleextract(width, height, depth);
		edgess[0] = edges;
		depths[0] = depth;

		for (n = 1; n < 6; n++) {
			if (depths[n] != NULL)
				free(depths[n]);
			if (depthmiddles[n] != NULL)
				free(depthmiddles[n]);
			depths[n] = halfscalealloc(width>>(n-1), height>>(n-1), depths[n-1]);
			depthmiddles[n] = middleextract(width>>n, height>>n, depths[n]);
			if (edgess[n] != NULL)
				free(edgess[n]);
			if (edgesmiddles[n] != NULL)
				free(edgesmiddles[n]);
			edgess[n] = halfscalealloc(width>>(n-1), height>>(n-1), edgess[n-1]);
			edgesmiddles[n] = middleextract(width>>n, height>>n, edgess[n]);
		}

		for (n = 0; n < 6; n++) {
			for (y = 0; y < 15; y++) {
				for (x = 0; x < 20; x++) {
					bbuf[y*width*4+(x+20*n)*4+1] = edgesmiddles[n][y*20+x];
				}
			}
			for (y = 0; y < 15; y++) {
				for (x = 0; x < 20; x++) {
					bbuf[y*width*4+(x+20*n)*4+2] = depthmiddles[n][y*20+x];
				}
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

		while (poll(fds, 1, 0) > 0) {
			strbuf[0] = '\0';
			if (fds[0].revents & POLLHUP)
				goto END;
			if (fds[0].revents & POLLIN) {
				n = read(fds[0].fd, strbuf, 16);
				if (n <= 0)
					goto END;
			}
			switch (strbuf[0]) {
				case 'w':
					for (n = 0; n < 100; n++)
						motors[n] = 0.0;
					for (n = 0; n < 20; n++)
						motors[n] = 1.0;
					break;
				case 'a':
					for (n = 0; n < 100; n++)
						motors[n] = 0.0;
					for (n = 20; n < 40; n++)
						motors[n] = 1.0;
					break;
				case 'd':
					for (n = 0; n < 100; n++)
						motors[n] = 0.0;
					for (n = 40; n < 60; n++)
						motors[n] = 1.0;
					break;
				case 'r':
					for (n = 0; n < 100; n++)
						motors[n] = 0.0;
					for (n = 60; n < 80; n++)
						motors[n] = 1.0;
					break;
				case 'f':
					for (n = 0; n < 100; n++)
						motors[n] = 0.0;
					for (n = 80; n < 100; n++)
						motors[n] = 1.0;
				default:
					break;
			}
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
		if (votes[1] > (votes[0] + votes[2] + votes[3] + votes[4]))
			printf("a\n");
		if (votes[2] > (votes[0] + votes[1] + votes[3] + votes[4]))
			printf("d\n");
		if (votes[3] > (votes[0] + votes[1] + votes[2] + votes[4])) {
			if (tilt < 30)
				tilt += 3;
		}
		if (votes[4] > (votes[0] + votes[1] + votes[2] + votes[3])) {
			if (tilt > -30)
				tilt -= 3;
		}
		if (tiltfile != NULL) {
			if (fprintf(tiltfile, "%d\n", tilt) < 0) {
				fclose(tiltfile);
				tiltfile = NULL;
			} else {
				fscanf(tiltfile, "%d %f %f %f\n", &tilt, &input[59003], &input[59004], &input[59005]);
			}
		}
		else {
			tiltfile = fopen(TILT, "a+");
		}
		fflush(stdout);

		// i don't get why this works
		for (n = 0; n < 100; n++) {
			motors[n] -= (motors[n] - avg) - (0.2 - avg) * fidget;
//			motors[n] -= (motors[n] - avg - 0.2) * fidget * motors[n] + 0.00001;
		}
		memcpy(&output[600], motors, 100 * sizeof(fann_type));

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

		for (n = 0; n < 60000; n++) {
			bbuf[(15*width+n)*4] = (unsigned char)(input[n] * 255.0);
		}

		if (running) {
			n = stat(LOCK, &statbuf);
			if (n < 0) {
				if (running && (gotsigchld == 0))
					n = fork();
				if (n == 0) {
					signal(SIGINT, SIG_IGN);
					creat(LOCK, S_IRWXU);
					file = fopen(STREAM, "ab");
					n = compressjpg(640, 480, bbuf, bjpeg);
					fprintf(file, "--myboundary\r\nContent-Type: image/jpeg\r\n\r\n");
					fwrite(bjpeg, 1, n, file);
					fprintf(file, "\r\n");
					fflush(file);
					fclose(file);
					unlink(LOCK);
					return 0;
				}
			}
			if (running) {
				while ((gotsigchld > 0) && (waitpid(-1, NULL, WNOHANG) > 0)) {
					gotsigchld--;
				}
			}
		}

		i = XCreateImage(d, DefaultVisual(d, s), DefaultDepth(d, s),
			ZPixmap, 0, 0, width, height, 32, 0);
		i->data = bbuf;

		XPutImage (d, w, gc, i, 0, 0, 0, 0, width, height);

		while (XCheckWindowEvent(d, w, ExposureMask | KeyPressMask, &e)) {
			//fprintf(stderr, "%p\n", &e);
			if (e.type == Expose)
				XPutImage (d, w, gc, i, 0, 0, 0, 0, width, height);

			if (e.type == KeyPress) {
				if (e.xkey.keycode == 9)
					running = 0;
			}
		}

		i->data = NULL;
		XDestroyImage(i);
	}

END:
	unlink(LOCK);
	XCloseDisplay(d);
	fann_save(ann, BRAIN);
	return 0;
}
