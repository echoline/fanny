#include <fann.h>

int
main(int argc, char **argv)
{
	if (argc < 2)
		return -1;
	struct fann *ann;
	ann = fann_create_standard(5, 8192, 1024, 1024, 1024, 1024);
	fann_set_activation_function_hidden(ann, FANN_LINEAR_PIECE_LEAKY);
	fann_set_activation_function_output(ann, FANN_SIGMOID);
	fann_save(ann, argv[1]);
	return 0;
}
