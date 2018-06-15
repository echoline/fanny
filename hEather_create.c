#include <fann.h>

int main() {
	struct fann *ann;
	ann = fann_create_standard(4, 60000, 1000, 1000, 1000);
	fann_save(ann, "/mnt/sdc1/anns/hEather.fann");
}
