#include <stdio.h>
#include "conv.h"

int main() {
	struct fann **anns;
	fann_type *input;
	fann_type *output;

	anns = conv_create(240, 120, 3, 3, 3, 2, 3, 7, 4, 1000, 1000, 2);
	input = calloc(115200, sizeof(fann_type));

	output = conv_run(anns, input);

	printf("%lf %lf\n", output[0], output[1]);
}

