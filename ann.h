typedef struct Ann Ann;
typedef struct Layer Layer;
typedef struct Neuron Neuron;
typedef struct Weights Weights;

struct Ann {
        int n;
        float rate;
        Layer **layers;
        Weights **weights;
        Weights **deltas;
        void *user;
};

struct Layer {
        int n;
        Neuron **neurons;
        float *values;
};

struct Neuron {
        float (*activation)(Neuron*);
        float (*gradient)(Neuron*);
        float steepness;
        float sum;
	float *value;
        void *user;
        void *internal;
};

struct Weights {
        int inputs;
        int outputs;
        float *values;
};

float activation_sigmoid(Neuron*);
float gradient_sigmoid(Neuron*);
float activation_tanh(Neuron*);
float gradient_tanh(Neuron*);
float activation_leaky_relu(Neuron*);
float gradient_leaky_relu(Neuron*);

Ann *anncreate(int, ...);
Ann *anncreatev(int, int*);
Layer *layercreate(int, float(*)(Neuron*), float(*)(Neuron*), float);
Neuron *neuroninit(Neuron*, float (*)(Neuron*), float (*)(Neuron*), float, float*);
Neuron *neuroncreate(float (*)(Neuron*), float (*)(Neuron*), float, float*);
Weights *weightsinitrand(Weights*);
Weights *weightsinitrandscale(Weights*, float);
Weights *weightsinitfloat(Weights*, float);
Weights *weightsinitfloats(Weights*, float*);
Weights *weightscreate(int, int, int);
float *annrun(Ann*, float*);
float anntrain(Ann*, float*, float*);

void annsave(Ann*, char*);
Ann *annload(char*);

