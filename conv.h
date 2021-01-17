#include <parallel_fann.h>

struct fann **conv_create(unsigned int, unsigned int, unsigned int, ...);
fann_type *conv_run(unsigned int, struct fann**, fann_type*);
void conv_train(unsigned int, struct fann**, fann_type*, fann_type*);

