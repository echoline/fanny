#include "ann.h"
#include <stdio.h>

int
main()
{
	int i, counter = 0;
	Ann *test = anncreate(3, 2, 4, 1);
	float inputs[4][2] = { { 1.0, 1.0 }, {1.0, -1.0}, {-1.0, 1.0}, {-1.0, -1.0}};
	float outputs[4] = { 0.0, 1.0, 1.0, 0.0 };
	float error = 1000;

	printf("testing anntrain()\n");
	while (error >= 0.001) {
		error = 0;
		for (i = 0; i < 4; i++)
			error += anntrain(test, inputs[i], &outputs[i]);	
		counter++;
		if (counter % 10000 == 1)
			printf("error: %f\n", error);
	}
	printf("error: %f, done after %d epochs\n", error, counter);

	return 0;
}

