#include <parallel_fann.h>

struct fann **conv_create(unsigned int kernels, unsigned int klayers, unsigned int layers, ...);
fann_type *conv_run(unsigned int kernels, struct fann **anns, fann_type *input);

