#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
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
#define BRAIN "/mnt/sdc1/anns/hEather.ann"

#include "ann.h"

struct Ann *ann = NULL;
unsigned char running = 1;

void
ctrlc(int sig) {
	if (sig == SIGINT)
		running = 0;
}

int
main(int argc, char **argv) {
	int x, y, o, n;
	int rc;
	unsigned char *depthmiddles[] = {NULL, NULL, NULL, NULL, NULL, NULL, };
	unsigned char *edgesmiddles[] = {NULL, NULL, NULL, NULL, NULL, NULL, };
	unsigned char *bbuf = malloc(3660);
	float *results;
	float deptherror, edgeserror;
	float lowestdeptherror, lowestedgeserror;
	int depthwinner, edgeswinner;
	FILE *tiltfile = NULL;
	int counter;
	int lowest;
	int highest;
	int lastlowest;
	int lasthighest = 0;
	float fidget, surprise, avg, sum;
	int tilt = 0;
	struct pollfd fds[1];
	float *input;
	float *output;
	float *motors;
	int *lasts;
	float *votes;
	char *strbuf;
	int eyesfd;

	eyesfd = open(EYES, O_RDONLY);
	if (eyesfd < 0)
		return -1;

	ann = annload(BRAIN);
	if (ann == NULL)
		return -1;

	input = calloc(60000, sizeof(float));
	output = calloc(1000, sizeof(float));
	motors = malloc(100 * sizeof(float));
	lasts = malloc(10 * sizeof(int));
	votes = malloc(5 * sizeof(float));
	strbuf = malloc(32);

	for (n = 0; n < 10; n++) {
		lasts[n] = 600;
	}

//	signal(SIGINT, &ctrlc);

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

		close(eyesfd);
		eyesfd = open(EYES, O_RDONLY);
		if (eyesfd < 0)
			return -1;

		for (n = 0; n < 6; n++) {
			if (depthmiddles[n] != NULL)
				free(depthmiddles[n]);
			if (edgesmiddles[n] != NULL)
				free(edgesmiddles[n]);
			depthmiddles[n] = malloc(20*15);
			edgesmiddles[n] = malloc(20*15);
			for (y = 0; y < 15; y++) for (x = 0; x < 20; x++) {
				depthmiddles[n][y*20+x] = bbuf[y*120+x+n*20+60+1800];
				edgesmiddles[n][y*20+x] = bbuf[y*120+x+n*20+60];
			}
		}

/*
		printf("%11s %11d %11d %11d %11d ", "k8", 0, 0, 20, 15);
		fflush(stdout);
		write(1, edgesmiddles[5], 20*15);
		fflush(stdout);
*/

		results = annrun(ann, input);
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
		memcpy(&output[700], &results[700], 300*sizeof(float));
		for (n = 0; n < 300; n++) {
			output[n] = depthmiddles[depthwinner][n] / 255.0;
			output[300+n] = edgesmiddles[edgeswinner][n] / 255.0;
		}

		memset(votes, 0, 5 * sizeof(int));
		memcpy(motors, &results[600], 100 * sizeof(float));

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
				annsave(ann, BRAIN);
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
		else {
			printf("s\n");
		}
		if (tiltfile != NULL) {
			if (fprintf(tiltfile, "%d\n", tilt) < 0) {
				fclose(tiltfile);
				tiltfile = NULL;
			} else {
				memmove(&input[59050], &input[59000], 950);
				fscanf(tiltfile, "%d %f %f %f\n", &tilt, &input[59002], &input[59003], &input[59004]);
			}
		}
		else {
			tiltfile = fopen(TILT, "a+");
		}
		fflush(stdout);

		// i don't get why this works
		for (n = 0; n < 100; n++)
			motors[n] -= (motors[n] - avg) - (0.2 - avg) * fidget;
//			motors[n] -= (motors[n] - avg) * sum * fidget + 0.01;

		memcpy(&output[600], motors, 100 * sizeof(float));

		memmove(&lasts[1], lasts, 9*sizeof(int));
		lasts[0] = anntrain(ann, input, output);

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
		fidget = counter / 10.0;
		if (fidget > 1.0)
			fidget = 1.0;
		surprise = (lasthighest + lastlowest) / 100.0;
		fprintf(stderr, "\r%d %d fidget: %.1f surprise: %.10f mvar: %.80f mavg: %.80f", edgeswinner, depthwinner, fidget, surprise, sum, avg);

		memmove(&input[3600], input, 32400*sizeof(float));
		for (y = 0; y < 3600; y++)
			input[y] = bbuf[y+60] / 255.0;
		memmove(&input[36000+2000], &input[36000], 21000*sizeof(float));
		memcpy(&input[36000], results, 1000*sizeof(float));
		memcpy(&input[36000+1000], ann->layers[2]->values, 1000 * sizeof(float));
		input[59000] = fidget;
		input[59001] = surprise;
		for (n = 0; n < 9; n++)
			memcpy(&input[59005+5*n], input, 5 * sizeof(float));
	}

END:
	annsave(ann, BRAIN);
	return 0;
}
