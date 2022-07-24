#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <omp.h>
#include "ann.h"

float
activation_sigmoid(Neuron *in)
{
	return 1.0/(1.0+exp(-in->sum));
}

float
gradient_sigmoid(Neuron *in)
{
	float y = *(in->value) * in->steepness;
	return y * (1.0 - y);
}

float
activation_tanh(Neuron *in)
{
	return tanh(in->sum);
}

float
gradient_tanh(Neuron *in)
{
	return (1.0 - (*(in->value))*(*(in->value))) * in->steepness;
}

float
activation_leaky_relu(Neuron *in)
{
	if (in->sum > 0)
		return in->sum;
	return in->sum * 0.01;
}

float
gradient_leaky_relu(Neuron *in)
{
	if (in->sum > 0)
		return in->steepness;
	return 0.01 * in->steepness;
}

Weights*
weightsinitfloats(Weights *in, float *init)
{
	int i, o;

	for (i = 0; i <= in->inputs; i++)
		for (o = 0; o < in->outputs; o++)
			in->values[i * in->outputs + o] = init[o];

	return in;
}

Weights*
weightsinitfloat(Weights *in, float init)
{
	int i, o;

	o = (in->inputs+1) * in->outputs;

	for (i = 0; i < o; i++)
		in->values[i] = init;

	return in;
}

Weights*
weightsinitrandscale(Weights *in, float scale)
{
	int i, o;

	o = (in->inputs+1) * in->outputs;

	srand(time(0));
	for (i = 0; i < o; i++)
		in->values[i] = (((float)rand()/RAND_MAX) - 0.5) * scale;

	return in;
}

Weights*
weightsinitrand(Weights *in)
{
	weightsinitrandscale(in, 0.2);
	return in;
}

Neuron*
neuroninit(Neuron *in, float (*activation)(Neuron*), float (*gradient)(Neuron*), float steepness, float *v)
{
	in->activation = activation;
	in->gradient = gradient;
	in->steepness = steepness;
	in->value = v;
	*in->value = 1.0;
	in->sum = 0;
	return in;
}

Neuron*
neuroncreate(float (*activation)(Neuron*), float (*gradient)(Neuron*), float steepness, float *v)
{
	Neuron *ret = calloc(1, sizeof(Neuron));
	neuroninit(ret, activation, gradient, steepness, v);
	return ret;
}

Layer*
layercreate(int num_neurons, float(*activation)(Neuron*), float(*gradient)(Neuron*), float rate)
{
	Layer *ret = calloc(1, sizeof(Layer));
	int i;

	ret->n = num_neurons;
	ret->neurons = calloc(num_neurons+1, sizeof(Neuron*));
	ret->values = calloc(num_neurons+1, sizeof(float));
	for (i = 0; i <= ret->n; i++) {
		ret->neurons[i] = neuroncreate(activation, gradient, rate, &ret->values[i]);
	}
	return ret;
}

Weights*
weightscreate(int inputs, int outputs, int initialize)
{
	Weights *ret = calloc(1, sizeof(Weights));
	ret->inputs = inputs;
	ret->outputs = outputs;
	ret->values = calloc(outputs * (inputs+1), sizeof(float));
	if (initialize)
		weightsinitrand(ret);
	return ret;
}

Ann*
anncreatev(int num_layers, int *v)
{
	Ann *ret = calloc(1, sizeof(Ann));
	int arg;
	int i;

	ret->n = num_layers;
	ret->rate = 0.7;
	ret->layers = calloc(num_layers, sizeof(Layer*));
	ret->weights = calloc(num_layers-1, sizeof(Weights*));
	ret->deltas = calloc(num_layers-1, sizeof(Weights*));

	for (i = 0; i < (num_layers-1); i++) {
		arg = v[i];
		if (arg < 0 || arg > 1000000)
			arg = 0;
		ret->layers[i] = layercreate(arg, activation_leaky_relu, gradient_leaky_relu, 0.5);
	}
	arg = v[i];
	if (arg < 0 || arg > 1000000)
		arg = 0;
	ret->layers[num_layers-1] = layercreate(arg, activation_sigmoid, gradient_sigmoid, 0.5);
	for (i = 0; i < (num_layers-1); i++) {
		ret->weights[i] = weightscreate(ret->layers[i]->n, ret->layers[i+1]->n, 1);
		ret->deltas[i] = weightscreate(ret->layers[i]->n, ret->layers[i+1]->n, 0);
	}

	return ret;
}

Ann*
anncreate(int num_layers, ...)
{
	Ann *ret = calloc(1, sizeof(Ann));
	va_list args;
	int arg;
	int i;

	va_start(args, num_layers);
	ret->n = num_layers;
	ret->rate = 0.7;
	ret->layers = calloc(num_layers, sizeof(Layer*));
	ret->weights = calloc(num_layers-1, sizeof(Weights*));
	ret->deltas = calloc(num_layers-1, sizeof(Weights*));

	for (i = 0; i < (num_layers-1); i++) {
		arg = va_arg(args, int);
		if (arg < 0 || arg > 1000000)
			arg = 0;
		ret->layers[i] = layercreate(arg, activation_leaky_relu, gradient_leaky_relu, 0.5);
	}
	arg = va_arg(args, int);
	if (arg < 0 || arg > 1000000)
		arg = 0;
	ret->layers[num_layers-1] = layercreate(arg, activation_sigmoid, gradient_sigmoid, 0.5);
	for (i = 0; i < (num_layers-1); i++) {
		ret->weights[i] = weightscreate(ret->layers[i]->n, ret->layers[i+1]->n, 1);
		ret->deltas[i] = weightscreate(ret->layers[i]->n, ret->layers[i+1]->n, 0);
	}

	va_end(args);

	return ret;
}

float*
annrun(Ann *ann, float *input)
{
	int l, i, o, n, m;
	int outputs = ann->layers[ann->n - 1]->n;
	float *ret = malloc(outputs * sizeof(float));
	Neuron *O;
	float sum;
	float *A, *B;

	for (i = 0; i < ann->layers[0]->n; i++)
		ann->layers[0]->values[i] = input[i];

	for (l = 1; l < ann->n; l++) {
		n = ann->layers[l]->n;
		for (o = 0; o < n; o++) {
			O = ann->layers[l]->neurons[o];
			A = ann->layers[l-1]->values;
			B = ann->weights[l-1]->values;

			sum = B[ann->weights[l-1]->inputs * n + o]; // bias

			m = ann->layers[l-1]->n;

			switch(m & 3) {
			case 3:
				sum += A[2] * B[2 * n + o];
			case 2:
				sum += A[1] * B[n + o];
			case 1:
				sum += A[0] * B[o];
			case 0:
				break;
			}

			#pragma omp parallel for reduction(+:sum)
			for (i = m & 3; i < m; i += 4)
				sum += A[i] * B[i * n + o] + A[i+1] * B[(i+1) * n + o] + A[i+2] * B[(i+2) * n + o] + A[i+3] * B[(i+3) * n + o];

			O->sum = sum * O->steepness;

			sum = 150/O->steepness;
			if (O->sum > sum)
				O->sum = sum;
			else if (O->sum < -sum)
				O->sum = -sum;

			*O->value = O->activation(O);
		}
	}

	for (o = 0; o < outputs; o++)
		ret[o] = ann->layers[ann->n - 1]->values[o];

	return ret;
}

float
anntrain(Ann *ann, float *inputs, float *outputs)
{
	float *error = annrun(ann, inputs);
	float ret = 0.0;
	int noutputs = ann->layers[ann->n-1]->n;
	float sum;
	int o, i, w, n, m;
	Neuron *O;
	Weights *W;
	Weights *D, *D2;
	float *A, *B, *C;

	for (o = 0; o < noutputs; o++) {
		// error = outputs[o] - result
		error[o] -= outputs[o];
		error[o] = -error[o];
		ret += pow(error[o], 2.0);
	}
	D = ann->deltas[ann->n-2];
	weightsinitfloats(D, error);
	for (i = 0; i < (ann->n-2); i++) {
		D = ann->deltas[i];
		weightsinitfloat(D, 1.0);
	}


	// backpropagate MSE
	D2 = ann->deltas[ann->n-2];
	for (w = ann->n-2; w >= 0; w--) {
		D = ann->deltas[w];

		for (o = 0; o < ann->layers[w+1]->n; o++) {
			O = ann->layers[w+1]->neurons[o];
			sum = 1.0;
			if (D2 != D) {
				W = ann->weights[w + 1];
				sum = 0.0;
				m = o * D2->outputs;
				A = &D2->values[m];
				B = &W->values[m];

				switch(D2->outputs & 3) {
				case 3:
					sum += A[2] * B[2];
				case 2:
					sum += A[1] * B[1];
				case 1:
					sum += A[0] * B[0];
				case 0:
					break;
				}

				m = D2->outputs;
				#pragma omp parallel for reduction(+:sum)
				for (n = m & 3; n < m; n += 4)
					sum += A[n] * B[n] + A[n + 1] * B[n + 1] + A[n + 2] * B[n + 2] + A[n + 3] * B[n + 3];
			}
			sum *= O->gradient(O);
			A = &D->values[o];
			n = ann->layers[w]->n;
			#pragma omp parallel for
			for (i = 0; i <= n; i++)
				A[i * D->outputs] *= sum;
		}

		D2 = D;
	}

	// update weights
	for (w = 0; w < ann->n-1; w++) {
		W = ann->weights[w];
		D = ann->deltas[w];
		A = ann->layers[w]->values;
		n = W->outputs;
		m = W->inputs;

		for (i = 0; i <= m; i++) {
			sum = ann->rate * A[i];
			o = i * W->outputs;
			B = &W->values[o];
			C = &D->values[o];

			#pragma omp parallel for
			for (o = 0; o < n; o++)
				B[o] += C[o] * sum;
		}
	}

	free(error);
	return ret;
}

void
annsave(Ann *ann, char *file) {
	int i, j, k, n;
	FILE *f = fopen(file, "w");
	fprintf(f, "%d ", ann->n);
	for (i = 0; i < ann->n; i++)
		fprintf(f, "%d ", ann->layers[i]->n);
	fprintf(f, "\n");
	for (i = 0; i < (ann->n-1); i++) {
		for(j = 0; j <= ann->layers[i]->n; j++) {
			n = ann->layers[i+1]->n;
			for (k = 0; k < n; k++)
				fprintf(f, "%f ", ann->weights[i]->values[j * n + k]);
		}
		fprintf(f, "\n");
	}
	fclose(f);
}

Ann*
annload(char *file) {
	int i, j, k;
	Ann *ann;
	int n;
	int *ls;
	FILE *f = fopen(file, "r");
	fscanf(f, "%d", &n);
	ls = malloc(n * sizeof(int));
	for (i = 0; i < n; i++)
		fscanf(f, "%d", &ls[i]);
	ann = anncreatev(n, ls);
	free(ls);
	for (i = 0; i < (ann->n-1); i++)
		for(j = 0; j <= ann->layers[i]->n; j++) {
			n = ann->layers[i+1]->n;
			for (k = 0; k < n; k++)
				if (fscanf(f, "%f", &(ann->weights[i]->values[j * n + k])) == EOF) {
					fclose(f);
					return ann;
				}
		}
	fclose(f);
	return ann;
}

