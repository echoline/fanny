#include <stdarg.h>
#include "conv.h"

struct fann**
conv_create(unsigned int kernels, unsigned int klayers, unsigned int layers, ...)
{
	unsigned int i;
	va_list args;
	struct fann **ret = calloc(kernels+1, sizeof(struct fann*));
	unsigned int *params;

	va_start(args, klayers+layers);

	params = calloc(klayers+1, sizeof(unsigned int));
	for(i = 0; i < klayers; i++)
		params[i] = va_arg(args, unsigned int);
	params[klayers] = 1;
	for(i = 0; i < kernels; i++)
		ret[i] = fann_create_standard_array(klayers+1, params);
	free(params);

	params = calloc(layers, sizeof(unsigned int));
	for(i = 0; i < layers; i++)
		params[i] = va_arg(args, unsigned int);
	ret[kernels] = fann_create_standard_array(layers, params);
	free(params);

	va_end(args);

	return ret;
}

fann_type*
conv_run(unsigned int kernels, struct fann **anns, fann_type *input)
{
	unsigned int i, j, k, s, e;
	struct fann *ann;
	fann_type *middle = calloc(anns[kernels]->num_input, sizeof(fann_type));
	fann_type *ret;

	s = anns[kernels]->num_input / kernels;
	for(k = 0; k < kernels; k++) {
		ann = anns[k];
		e = s - ann->num_input;
		for (j = 0; j < e; j++) {
			ret = fann_run(ann, &input[j]);
			middle[s * k + j] = ret[0];
		}
	}

	ret = fann_run(anns[kernels], middle);
	free(middle);
	return ret;
}

void
conv_compute_MSE(unsigned int kernels, struct fann **anns, fann_type *desired_output)
{
	unsigned int j, e, k, s;
	struct fann *ann;
	fann_type *o = desired_output;

	fann_compute_MSE(anns[kernels], desired_output);

	s = anns[kernels]->num_input / kernels;
	for(k = 0; k < kernels; k++) {
		ann = anns[k];
		e = s - ann->num_input;
		for (j = 0; j < e; j++) {
		}
	}
}

void
conv_train(unsigned int kernels, struct fann **anns, fann_type *input, fann_type *desired_output)
{
	conv_run(kernels, anns, inputs, desired_output);
	conv_compute_MSE(kernels, anns, desired_output);
	conv_backpropagate_MSE(kernels, anns);
	conv_update_weights(kernels, anns);
}

