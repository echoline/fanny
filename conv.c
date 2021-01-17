#include <stdarg.h>
#include "conv.h"

struct fann**
conv_create(unsigned int x, unsigned int y,
	    unsigned int kernels, unsigned int kx, unsigned int ky,
	    unsigned int klayers, unsigned int layers, ...)
{
	unsigned int i;
	va_list args;
	struct fann **ret = calloc(kernels+1, sizeof(struct fann*));
	unsigned int *params;
	struct fann_conv *conv;

	conv = calloc(1, sizeof(struct fann_conv));
	conv->kernels = kernels;
	conv->x = x;
	conv->y = y;
	conv->kx = kx;
	conv->ky = ky;
	conv->middle_data = calloc(x * y, sizeof(fann_type));
	conv->middle_layer = calloc(1, sizeof(struct fann_layer));

	va_start(args, klayers+layers);

	params = calloc(klayers+1, sizeof(unsigned int));
	for(i = 0; i < klayers; i++)
		params[i] = va_arg(args, unsigned int);
	params[klayers] = 1;
	for(i = 0; i < kernels; i++) {
		ret[i] = fann_create_standard_array(klayers+1, params);
		ret[i]->user_data = conv;
	}
	free(params);

	params = calloc(layers, sizeof(unsigned int));
	for(i = 0; i < layers; i++)
		params[i] = va_arg(args, unsigned int);
	ret[kernels] = fann_create_standard_array(layers, params);
	free(params);

	va_end(args);

	ret[kernels]->user_data = conv;

	return ret;
}

fann_type*
conv_run(struct fann **anns, fann_type *input)
{
	unsigned int i, j, k, s, x, y, mx, my;
	struct fann *ann;
	fann_type *ret;
	fann_type *tmp;
	struct fann_conv *conv;

	conv = anns[0]->user_data;
	s = anns[0]->num_input / conv->kernels;
	tmp = calloc(conv->kx * conv->ky, sizeof(fann_type));
	mx = conv->x - conv->kx;
	my = conv->y - conv->ky;

	for(k = 0; k < conv->kernels; k++) {
		ann = anns[k];
		for (j = 0; j < my; j++)
			for (i = 0; i < mx; i++) {
				for (y = 0; y < conv->ky; y++)
					for (x = 0; x < conv->kx; x++)
						tmp[x * conv->ky + y] = input[(j + y) * conv->x + (i + x)];
				ret = fann_run(ann, tmp);
				conv->middle_data[s * k + j] = ret[0];
			}
	}

	ret = fann_run(anns[conv->kernels], conv->middle_data);
	free(tmp);
	return ret;
}

void
conv_compute_MSE(struct fann **anns, fann_type *desired_output)
{
	unsigned int j, e, k, s;
	struct fann *ann;
	fann_type *o = desired_output;
	struct fann_conv *conv = anns[0]->user_data;
	fann_type *middle = anns[conv->kernels]->user_data;

	fann_compute_MSE(anns[conv->kernels], desired_output);

	s = anns[conv->kernels]->num_input / conv->kernels;
	for(k = 0; k < conv->kernels; k++) {
		ann = anns[k];
		e = s - ann->num_input;
		for (j = 0; j < e; j++) {
		}
	}
}

void
conv_backpropagate_MSE(struct fann **anns)
{
}

void
conv_update_weights(struct fann **anns)
{
}

void
conv_train(struct fann **anns, fann_type *input, fann_type *desired_output)
{
	conv_run(anns, input);
	conv_compute_MSE(anns, desired_output);
	conv_backpropagate_MSE(anns);
	conv_update_weights(anns);
}

