#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include "ann.h"

float
activation_sigmoid(Neuron *in)
{
	return 1.0/(1.0+exp(-in->sum));
}

float
gradient_sigmoid(Neuron *in)
{
	float y = in->value;
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
	return 1.0 - in->value*in->value;
}

float
activation_leaky_relu(Neuron *in)
{
	if (in->sum > 10000.0)
		in->sum = 10000.0;
	if (in->sum > 0)
		return in->sum;
	if (in->sum < -10000.0)
		in->sum = -10000.0;
	return in->sum * 0.01;
}

float
gradient_leaky_relu(Neuron *in)
{
	if (in->sum > 0)
		return 1.0;
	return 0.01;
}

Weights*
weightsinitfloats(Weights *in, float *init)
{
	int i, o;

	for (i = 0; i <= in->inputs; i++)
		for (o = 0; o < in->outputs; o++)
			in->values[i][o] = init[o];

	return in;
}

Weights*
weightsinitfloat(Weights *in, float init)
{
	int i, o;

	for (i = 0; i <= in->inputs; i++)
		for (o = 0; o < in->outputs; o++)
			in->values[i][o] = init;

	return in;
}

Weights*
weightsinitrandscale(Weights *in, float scale)
{
	int i, o;

	srand(time(0));
	for (i = 0; i <= in->inputs; i++)
		for (o = 0; o < in->outputs; o++)
			in->values[i][o] = (((float)rand()/RAND_MAX) - 0.5) * scale;

	return in;
}

Weights*
weightsinitrand(Weights *in)
{
	weightsinitrandscale(in, 2.0);
	return in;
}

Neuron*
neuroninit(Neuron *in, float (*activation)(Neuron*), float (*gradient)(Neuron*), float steepness)
{
	in->activation = activation;
	in->gradient = gradient;
	in->steepness = steepness;
	in->value = 1.0;
	in->sum = 0;
	return in;
}

Neuron*
neuroncreate(float (*activation)(Neuron*), float (*gradient)(Neuron*), float steepness)
{
	Neuron *ret = calloc(1, sizeof(Neuron));
	neuroninit(ret, activation, gradient, steepness);
	return ret;
}

Layer*
layercreate(int num_neurons, float(*activation)(Neuron*), float(*gradient)(Neuron*), float rate)
{
	Layer *ret = calloc(1, sizeof(Layer));
	int i;

	ret->n = num_neurons;
	ret->neurons = calloc(num_neurons+1, sizeof(Neuron*));
	for (i = 0; i <= ret->n; i++) {
		ret->neurons[i] = neuroncreate(activation, gradient, rate);
	}
	return ret;
}

Weights*
weightscreate(int inputs, int outputs, int initialize)
{
	int i;
	Weights *ret = calloc(1, sizeof(Weights));
	ret->inputs = inputs;
	ret->outputs = outputs;
	ret->values = calloc(inputs+1, sizeof(float*));
	for (i = 0; i <= inputs; i++)
		ret->values[i] = calloc(outputs, sizeof(float));
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
	ret->rate = 0.25;
	ret->layers = calloc(num_layers, sizeof(Layer*));
	ret->weights = calloc(num_layers-1, sizeof(Weights*));
	ret->deltas = calloc(num_layers-1, sizeof(Weights*));

	for (i = 0; i < (num_layers-1); i++) {
		arg = v[i];
		if (arg < 0 || arg > 1000000)
			arg = 0;
		ret->layers[i] = layercreate(arg, activation_leaky_relu, gradient_leaky_relu, 1.0);
	}
	arg = v[i];
	if (arg < 0 || arg > 1000000)
		arg = 0;
	ret->layers[num_layers-1] = layercreate(arg, activation_sigmoid, gradient_sigmoid, 1.0);
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
	ret->rate = 0.25;
	ret->layers = calloc(num_layers, sizeof(Layer*));
	ret->weights = calloc(num_layers-1, sizeof(Weights*));
	ret->deltas = calloc(num_layers-1, sizeof(Weights*));

	for (i = 0; i < (num_layers-1); i++) {
		arg = va_arg(args, int);
		if (arg < 0 || arg > 1000000)
			arg = 0;
		ret->layers[i] = layercreate(arg, activation_leaky_relu, gradient_leaky_relu, 1.0);
	}
	arg = va_arg(args, int);
	if (arg < 0 || arg > 1000000)
		arg = 0;
	ret->layers[num_layers-1] = layercreate(arg, activation_sigmoid, gradient_sigmoid, 1.0);
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
	int l, i, o;
	int outputs = ann->layers[ann->n - 1]->n;
	float *ret = calloc(outputs, sizeof(float));
	Neuron *O;
	float sum;
	float *rhs;
	size_t sz;
	size_t nwg;
	float *C;

	for (i = 0; i < ann->layers[0]->n; i++)
		ann->layers[0]->neurons[i]->value = input[i];

	for (l = 1; l < ann->n; l++) {
		for (o = 0; o < ann->layers[l]->n; o++) {
			O = ann->layers[l]->neurons[o];
			O->sum = ann->weights[l-1]->values[ann->weights[l-1]->inputs][o]; // bias
			sum = O->sum;
			for (i = 0; i < ann->layers[l-1]->n; i++)
				sum += ann->layers[l-1]->neurons[i]->value * ann->weights[l-1]->values[i][o];

			O->sum = sum;
			O->value = O->activation(O);
		}
	}

	for (o = 0; o < outputs; o++)
		ret[o] = ann->layers[ann->n - 1]->neurons[o]->value;

	return ret;
}

float
anntrain(Ann *ann, float *inputs, float *outputs)
{
	float *error = annrun(ann, inputs);
	float ret = 0.0;
	int noutputs = ann->layers[ann->n-1]->n;
	float acc, sum;
	int o, i, w, n;
	Neuron *O, *I;
	Weights *W, *D, *D2;

	for (o = 0; o < noutputs; o++) {
		// error = outputs[o] - result
		error[o] -= outputs[o];
		error[o] = -error[o];
		ret += pow(error[o], 2.0) * 0.5;
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
			acc = O->gradient(O) * O->steepness;
			sum = 1.0;
			if (D2 != D) {
				W = ann->weights[w + 1];
				sum = 0.0;
				for (n = 0; n < D2->outputs; n++)
					sum += D2->values[o][n] * W->values[o][n];
			}
			for (i = 0; i <= ann->layers[w]->n; i++) {
			 	D->values[i][o] *= acc * sum;
			}
		}

		D2 = D;
	}

	// update weights
	for (w = 0; w < ann->n-1; w++) {
		W = ann->weights[w];
		D = ann->deltas[w];

		for (i = 0; i <= W->inputs; i++) {
			I = ann->layers[w]->neurons[i];
			for (o = 0; o < W->outputs; o++) {
				W->values[i][o] += D->values[i][o] * ann->rate * I->value;
			}
		}
	}

	free(error);
	return ret;
}

Ann*
adaminit(Ann *ann)
{
	int i;
	Adam *I = calloc(1, sizeof(Adam));

	I->rate = 0.001;
	I->beta1 = 0.9;
	I->beta2 = 0.999;
	I->epsilon = 10e-8;
	I->timestep = 0;
	I->first = calloc(ann->n-1, sizeof(Weights*));
	I->second = calloc(ann->n-1, sizeof(Weights*));

	for (i = 0; i < (ann->n-1); i++) {
		I->first[i] = weightscreate(ann->layers[i]->n, ann->layers[i+1]->n, 0);
		I->second[i] = weightscreate(ann->layers[i]->n, ann->layers[i+1]->n, 0);
	}

	ann->internal = I;

	return ann;
}

float
anntrain_adam(Ann *ann, float *inputs, float *outputs)
{
	float *error = annrun(ann, inputs);
	float ret = 0.0;
	int noutputs = ann->layers[ann->n-1]->n;
	float acc, sum, m, v;
	int o, i, w, n;
	Neuron *O, *I;
	Weights *W, *D, *D2, *M, *V;
	Adam *annI;

	if (ann->internal == 0)
		adaminit(ann);
	annI = ann->internal;
	annI->timestep++;

	for (o = 0; o < noutputs; o++) {
		// error = outputs[o] - result
		error[o] -= outputs[o];
		error[o] = -error[o];
		ret += pow(error[o], 2.0) * 0.5;
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
		M = annI->first[w];
		V = annI->second[w];

		for (o = 0; o < ann->layers[w+1]->n; o++) {
			O = ann->layers[w+1]->neurons[o];
			acc = O->gradient(O) * O->steepness;
			sum = 1.0;
			if (D2 != D) {
				W = ann->weights[w+1];
				sum = 0.0;
				for (n = 0; n < D2->outputs; n++)
					sum += D2->values[o][n] * W->values[o][n];
			}
			for (i = 0; i <= ann->layers[w]->n; i++) {
				I = ann->layers[w]->neurons[i];
			 	D->values[i][o] *= acc * sum;
				M->values[i][o] *= annI->beta1;
				M->values[i][o] += (1.0 - annI->beta1) * D->values[i][o] * I->value;
				V->values[i][o] *= annI->beta2;
				V->values[i][o] += (1.0 - annI->beta2) * D->values[i][o] * D->values[i][o] * I->value * I->value;
			}
		}

		D2 = D;
	}

	// update weights
	for (w = 0; w < ann->n-1; w++) {
		W = ann->weights[w];
		M = annI->first[w];
		V = annI->second[w];

		for (i = 0; i <= W->inputs; i++) {
			for (o = 0; o < W->outputs; o++) {
				m = M->values[i][o] / (annI->timestep < 100? (1.0 - pow(annI->beta1, annI->timestep)): 1.0);
				v = V->values[i][o] / (annI->timestep < 10000? (1.0 - pow(annI->beta2, annI->timestep)): 1.0);
				W->values[i][o] += (m / (sqrt(v) + annI->epsilon)) * annI->rate;
			}
		}
	}

	free(error);
	return ret;
}

float
anntrain_adamax(Ann *ann, float *inputs, float *outputs)
{
	float *error = annrun(ann, inputs);
	float ret = 0.0;
	int noutputs = ann->layers[ann->n-1]->n;
	float acc, sum, m, v;
	int o, i, w, n;
	Neuron *O, *I;
	Weights *W, *D, *D2, *M, *V;
	Adam *annI;

	if (ann->internal == 0)
		adaminit(ann);
	annI = ann->internal;
	annI->rate = 0.002;
	annI->timestep++;

	for (o = 0; o < noutputs; o++) {
		// error = outputs[o] - result
		error[o] -= outputs[o];
		error[o] = -error[o];
		ret += pow(error[o], 2.0) * 0.5;
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
		M = annI->first[w];
		V = annI->second[w];

		for (o = 0; o < ann->layers[w+1]->n; o++) {
			O = ann->layers[w+1]->neurons[o];
			acc = O->gradient(O) * O->steepness;
			sum = 1.0;
			if (D2 != D) {
				W = ann->weights[w+1];
				sum = 0.0;
				for (n = 0; n < D2->outputs; n++)
					sum += D2->values[o][n] * W->values[o][n];
			}
			for (i = 0; i <= ann->layers[w]->n; i++) {
				I = ann->layers[w]->neurons[i];
			 	D->values[i][o] *= acc * sum;
				M->values[i][o] *= annI->beta1;
				M->values[i][o] += (1.0 - annI->beta1) * D->values[i][o] * I->value;
				V->values[i][o] = fmax(V->values[i][o] * annI->beta2, fabs(D->values[i][o] * I->value));
			}
		}

		D2 = D;
	}

	// update weights
	for (w = 0; w < ann->n-1; w++) {
		W = ann->weights[w];
		M = annI->first[w];
		V = annI->second[w];

		for (i = 0; i <= W->inputs; i++) {
			for (o = 0; o < W->outputs; o++) {
				m = M->values[i][o];
				v = V->values[i][o];
				W->values[i][o] += (annI->rate/(1.0 - (annI->timestep < 100? pow(annI->beta1, annI->timestep): 0.0))) * (m/v);
			}
		}
	}

	free(error);
	return ret;
}

/*int
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

	error = 1000;
	counter = 0;
	for (i = test->n-2; i >= 0; i--)
		weightsinitrand(test->weights[i]);

	printf("testing anntrain_adam()\n");
	while (error >= 0.001) {
		error = 0;
		for (i = 0; i < 4; i++)
			error += anntrain_adam(test, inputs[i], &outputs[i]);	
		counter++;
		if (counter % 10000 == 1)
			printf("error: %f\n", error);
	}
	printf("error: %f, done after %d epochs\n", error, counter);

	error = 1000;
	counter = 0;
	for (i = test->n-2; i >= 0; i--)
		weightsinitrand(test->weights[i]);

	printf("testing anntrain_adamax()\n");
	while (error >= 0.001) {
		error = 0;
		for (i = 0; i < 4; i++)
			error += anntrain_adamax(test, inputs[i], &outputs[i]);	
		counter++;
		if (counter % 10000 == 1)
			printf("error: %f\n", error);
	}
	printf("error: %f, done after %d epochs\n", error, counter);
}*/

void
annsave(Ann *ann, char *file) {
	int i, j, k;
	FILE *f = fopen(file, "w");
	fprintf(f, "%d ", ann->n);
	for (i = 0; i < ann->n; i++)
		fprintf(f, "%d ", ann->layers[i]->n);
	fprintf(f, "\n");
	for (i = 0; i < (ann->n-1); i++) {
		for(j = 0; j <= ann->layers[i]->n; j++)
			for (k = 0; k < ann->layers[i+1]->n; k++)
				fprintf(f, "%f ", ann->weights[i]->values[j][k]);
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
		for(j = 0; j <= ann->layers[i]->n; j++)
			for (k = 0; k < ann->layers[i+1]->n; k++)
				if (fscanf(f, "%f", &(ann->weights[i]->values[j][k])) == EOF) {
					fclose(f);
					return ann;
				}
	fclose(f);
	return ann;
}

