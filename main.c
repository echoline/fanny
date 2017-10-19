#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <fann.h>
#include <pthread.h>
#include <signal.h>
#include "comms.h"

#define KWIDTH 200
#define KHEIGHT 150
#define KSIZE KWIDTH*KHEIGHT

int
decompressjpg(unsigned long jsize, unsigned char *jbuf, unsigned long bsize, unsigned char *bbuf);

typedef struct threadargs_t {
	char training;
	fann_type *outputs;
	int nfd;
} threadargs_t;

struct js_event {
	unsigned int time;
	short value;
	unsigned char type;
	unsigned char number;
};

void*
do_loop(void *data)
{
	threadargs_t *args = data;
	fann_type *outputs = args->outputs;
	struct js_event jse;
	double v, w, x, y, t, l, r, hfd;
	static char tf = 0;
	static char gamepadon = 0;
	int jfd = open("/dev/input/js0", O_RDONLY);
	if (jfd < 0) {
		fprintf(stderr, "error: open js0\n");
		pthread_exit(NULL);
	}

	while(1) {
		if (read(jfd, &jse, sizeof(jse)) == sizeof(jse)) {
//			fprintf(stderr, "%d %d %d\n", jse.type, jse.number,
//				jse.value);
			if (jse.type == 2 && jse.number & 2) {
				if (!gamepadon)
					continue;
				if (jse.number == 3) {
					y = (jse.value / -32768.0);
				} else if (jse.number == 2) {
					x = (jse.value / -32768.0);
				}
				v = (1.0 - fabs(x)) * (y/1.0) + y;
				w = (1.0 - fabs(y)) * (x/1.0) + x;
				r = ((v+w)/2.0);
				l = ((v-w)/2.0);
				outputs[0] = outputs[1] = l/4.0;
				outputs[2] = outputs[3] = -l/4.0;
				outputs[4] = outputs[5] = r/4.0;
				outputs[6] = outputs[7] = -r/4.0;
				if (args->nfd > 0)
					dprintf(args->nfd, "l%d r%d\n",
					    (int)(l * 100.0), (int)(r * 100.0));
//				fprintf(stderr, "l%d r%d\n",
//				    (int)(l * 100.0), (int)(r * 100.0));
			}	
			if (jse.type == 2 && jse.number == 1) {		   
				if (tf || !gamepadon)
					continue;
				v = jse.value / 32768.0;
				if (v < 0) {
					v *= 0.5;
				} else {
					v *= 1.5;
				}
				v -= 0.5;
				t = v;
				outputs[8] = outputs[9] = v / 4.0;
				outputs[10] = outputs[11] = -v / 4.0;
				hfd = open("/home/eli/kinect/tilt", O_WRONLY);
				if (hfd > 0) {
					dprintf(hfd, "%d\n", (int)(t*30.0));
//					fprintf(stderr, "%d\n", (int)(t*30.0));
					close(hfd);
				}
			}
			if (jse.type==1 && jse.number==3 && jse.value==0){
				if (!gamepadon)
					continue;
				tf = !tf;
				fprintf(stderr, "tilt: %s\n", tf? "off": "on");
			}
			if (jse.type==1 && jse.number==2 && jse.value==0){
				args->training = !args->training;
				fprintf(stderr, "training: %s\n", args->training? "on": "off");
			}
			if (jse.type==1 && jse.number==9 && jse.value==0){
				gamepadon = !gamepadon;
				fprintf(stderr, "gamepad: %s\n", gamepadon? "on": "off");
			}
		}
	}

	pthread_exit(NULL);
}

struct fann *ann;

void
sigint(int sig)
{
	fprintf(stderr, "saving... ");
	fflush(stderr);
	fann_save(ann, "baby.net");
	fprintf(stderr, "done\n");
}

int
main(int argc, char **argv)
{
	unsigned char bbuf[KSIZE];
	unsigned char jbuf[KSIZE];
	int fd, i, j, k, c, x, y, hfd;
	fann_type t, l, r;
	int thr_id;
	pthread_t p_thread;
	threadargs_t args;

	args.training = 0;
	for (i = 1; i < argc; i++)
		if (!strcmp(argv[i], "-h")) {
			fprintf(stderr, "%s [-ht]\n", argv[0]);
			return -1;
		} else if (!strcmp(argv[i], "-t")) {
			args.training = 1;
		}

	fann_type inputs[KSIZE];
	fann_type tmp[100*75];
	fann_type *output;
	fann_type outputs[12];
	struct fann_layer *layer;
	struct fann_neuron *neuron;
	struct fann_train_data *data;

	outputs[0] = outputs[1] = outputs[2] = outputs[3] = outputs[4] = outputs[5] = outputs[6] = outputs[7] = 0.0f;
	outputs[8] = outputs[9] = -0.125f;
	outputs[10] = outputs[11] = 0.125f;
	args.outputs = outputs;

	ann = fann_create_from_file("baby.net");
	fprintf(stderr, "fann loaded\n");

	signal(SIGINT, &sigint);
	thr_id = pthread_create(&p_thread, NULL, do_loop, (void*)&args);

	k = 0;
	args.nfd = init_connection("192.168.1.6:2001");
	while (1) {
		system("cat /home/eli/kinect/extra.jpg > /tmp/tmp.jpg");
		system("convert /tmp/tmp.jpg -resize 200 /tmp/new.jpg");

		fd = open("/tmp/new.jpg", O_RDONLY);
		if (fd < 0) {
			fprintf(stderr, "error: open new.jpg\n");
			return -1;
		}
		i = 0;
		while ((c = read(fd, jbuf + i, KSIZE - i)) > 0) {
			i += c;
		}
		if (i == 0) {
			fprintf(stderr, "error: read new.jpg\n");
			return -1;
		}
		decompressjpg(i, jbuf, KSIZE, bbuf);
		j = 0;
		for (y = 0; y < KHEIGHT; y++) for (x = 0; x < KWIDTH; x++) {
			i = y * KWIDTH + x;
			if (y >= (KHEIGHT/2) && x > (KWIDTH/2))
				tmp[j++] = inputs[i];
			inputs[i] = bbuf[i] / 256.0;
		}

		if (args.training != 0) {
			data = fann_create_train(1, KSIZE, 12);
			memcpy(*(data->input), inputs, KSIZE * sizeof(fann_type));
			memcpy(*(data->output), outputs, 12 * sizeof(fann_type));
			fann_train_on_data(ann, data, 5, 1, 0.0001);
			fann_destroy_train(data);
		}

		output = fann_run(ann, inputs);
		t = output[8] + output[9] - output[10] - output[11];
		l = output[0] + output[1] - output[2] - output[3];
		r = output[4] + output[5] - output[6] - output[7];
		fprintf(stderr, "%f %f %f\n", l, r, t);
		hfd = open("/home/eli/kinect/tilt", O_WRONLY);
		if (hfd > 0) {
			dprintf(hfd, "%d\n", (int)(t * 30.0));
			close(hfd);
		}
		dprintf(args.nfd, "l%d r%d\n", (int)(l * 100.0), (int)(r * 100.0));

		memmove(&tmp[300+12+2], tmp, sizeof(tmp)-((300+12+2)*sizeof(fann_type)));
		j = 0;
		for(layer = &ann->first_layer[3]; layer < ann->last_layer; layer = &layer[1]) {
			for (neuron = layer->first_neuron; neuron < layer->last_neuron; neuron = &neuron[1]) {
				tmp[j++] = neuron->value;
			}
		}
		fprintf(stderr, "j: %d\n", j);
		for (y = 0; y < (KHEIGHT/2); y++) for (x = 0; x < (KWIDTH/2); x++) {
			inputs[(y+(KHEIGHT/2)) * KWIDTH + (x+(KWIDTH/2))] = tmp[y * (KWIDTH/2) + x];
		}
	}
}

