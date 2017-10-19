#include <fann.h>

int
main() {
	struct fann *ann = fann_create_standard(4, 30000, 1000, 300, 12);
	fann_set_activation_function_hidden(ann, FANN_SIGMOID_SYMMETRIC);
	fann_set_activation_function_output(ann, FANN_SIGMOID_SYMMETRIC);

	fann_save(ann, "baby.net");

	fann_destroy(ann);
}
