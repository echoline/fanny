#include <X11/Xlib.h>
#include <X11/IntrinsicP.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <jpeglib.h>
#include <math.h>
#include <parallel_fann.h>
#include <signal.h>
#include <poll.h>
#define EYES "/home/eli/kinect/extra.jpg"
#define TILT "/home/eli/kinect/tilt"
#define BRAIN "/mnt/sdc1/anns/hEather.fann"

struct fann *ann = NULL;

void
ctrlc(int sig) {
	if (sig == SIGINT) {
		if (ann != NULL)
			fann_save(ann, BRAIN);
		exit(0);
	}
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
	unsigned char bbuf[640*480*4];
	unsigned char depth[640*480*4];
	unsigned char edges[640*480*4];
	unsigned char bandw[640*480*4];
	unsigned char *depths[] = {NULL, NULL, NULL, NULL, NULL, NULL, };
	unsigned char *edgess[] = {NULL, NULL, NULL, NULL, NULL, NULL, };
	unsigned char *depthmiddles[] = {NULL, NULL, NULL, NULL, NULL, NULL, };
	unsigned char *edgesmiddles[] = {NULL, NULL, NULL, NULL, NULL, NULL, };
	fann_type input[60000];
	fann_type output[1000];
	fann_type motors[100];
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
	int lasts[100];
	int lastlowest;
	int lasthighest;
	fann_type fidget, surprise, avg, sum;
	fann_type votes[5];
	int tilt = 0;
	struct pollfd fds[1];
	char strbuf[32];

	ann = fann_create_from_file(BRAIN);
	if (ann == NULL)
		return -1;
	fann_set_activation_function_hidden(ann, FANN_LINEAR_PIECE_LEAKY);
	memset(input, 0, 60000*sizeof(fann_type));
	memset(output, 0, 1000*sizeof(fann_type));
	for (n = 0; n < 100; n++) {
		lasts[n] = 600;
	}

	signal(SIGINT, &ctrlc);

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
		exit(1);
	}
 
	s = DefaultScreen(d);
	w = XCreateSimpleWindow(d, RootWindow(d, s), 0, 0, width, height, 1,
					BlackPixel(d, s), BlackPixel(d, s));
	XSelectInput(d, w, ExposureMask | KeyPressMask);
	XMapWindow(d, w);
	gc = DefaultGC (d, s);

	while (1) {
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
				dy = abs(y - hheight);
				jpeg_read_scanlines(&cinfo, &jbuf, 1);
				for (x = 0; x < width; x++) {
					dx = abs(x - hwidth);
					n = 0;
					if (dx < 160.0 && dy < 120.0)
						if (dx < 80.0 && dy < 60.0)
							if (dx < 40.0 && dy < 30)
								if (dx < 20.0 && dy < 15.0)
									n = 255;
								else
									n = 128;
							else
								n = 255;
						else
							n = 128;
					o = x + y*width;
					bbuf[o*4] = n;
					edges[o] = bbuf[o*4+1] = jbuf[x*3];
					depth[o] = bbuf[o*4+2] = jbuf[x*3+2];
					bandw[o] = jbuf[x*3];
				}
			}
		} else {
			exit(1);
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

		fds[0].fd = 0;
		fds[0].events = POLLIN | POLLHUP;
		while (poll(fds, 1, 0) == 1) {
			if (fds[0].revents & POLLHUP)
				ctrlc(SIGINT);
			n = read(fds[0].fd, strbuf, 16);
			if (n <= 0)
				ctrlc(SIGINT);
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

		i = XCreateImage(d, DefaultVisual(d, s), DefaultDepth(d, s),
			ZPixmap, 0, bbuf, width, height, 32, width * 4);

		XPutImage (d, w, gc, i, 0, 0, 0, 0, width, height);

		while (XCheckWindowEvent(d, w, ExposureMask | KeyPressMask, &e)) {
			//fprintf(stderr, "%p\n", &e);
			if (e.type == Expose)
				XPutImage (d, w, gc, i, 0, 0, 0, 0, width, height);

			if (e.type == KeyPress) {
				if (e.xkey.keycode == 9)
					break;
			}
		}

		XDestroyImage(i);
	}
 
	XCloseDisplay(d);
	return 0;
}
