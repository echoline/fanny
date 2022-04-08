#include "ann.h"

int
main() {
	Ann *ann = anncreate(5, 60000, 1000, 1000, 1000, 1000);

	annsave(ann, "/mnt/sdc1/anns/hEather.ann");
}
