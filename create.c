#include <fann.h>

int
main() {
	struct fann *ann = fann_create_standard(5, 60000, 1000, 1000, 1000, 1000);
	fann_set_activation_function_hidden(ann, FANN_LINEAR_PIECE_LEAKY);
	fann_set_activation_function_output(ann, FANN_SIGMOID);

	fann_save(ann, "/mnt/sdc1/anns/hEather.fann");

	fann_destroy(ann);
}
