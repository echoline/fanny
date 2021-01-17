#include <parallel_fann.h>

struct fann **conv_create(unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, ...);
fann_type *conv_run(struct fann**, fann_type*);
void conv_train(struct fann**, fann_type*, fann_type*);

struct fann_conv {
	unsigned int kernels;
	unsigned int x;
	unsigned int y;
	unsigned int kx;
	unsigned int ky;
	fann_type *middle_data;
	struct fann_layer *middle_layer;
};

