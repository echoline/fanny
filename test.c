#include <stdio.h>
#include "conv.h"

int main() {
	struct fann **anns;
	fann_type *input;
	fann_type *output;

	anns = conv_create(5, 2, 3, 7, 4, 9000, 1000, 2);
	input = calloc(115200, sizeof(fann_type));

	output = conv_run(5, anns, input);

	printf("%lf %lf\n", output[0], output[1]);
}

