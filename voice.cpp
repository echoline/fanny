#include <iostream>
#include <fann.h>
#include <csignal>
#include <cstring>

#define BRAIN "/mnt/sdc1/anns/voice.fann"

unsigned char running = 1;

void
ctrlc(int sig) {
	if (sig == SIGINT)
		running = 0;
}

int
main() {
	struct fann *ann = fann_create_from_file(BRAIN);
	if (ann == NULL)
		return -1;                                                      
	unsigned char c;
	int i;
	fann_type input[1000];
	fann_type output[1000];
	fann_type buf[100];
	fann_type *results;
	struct fann_neuron *neuron;

	fann_set_activation_function_hidden(ann, FANN_LINEAR_PIECE_LEAKY);
	fann_set_activation_function_output(ann, FANN_LINEAR_PIECE_LEAKY);
	signal(SIGINT, &ctrlc);

	while(running) {
		results = fann_run(ann, input);
		for (i = 0; i < 100; i++) {
			std::cin >> c;
			buf[i] = c / 128.0;
			output[i] = buf[i];
		}
		for (i = 100; i < 1000; i++) {
			output[i] = results[i];
		}
		for (i = 100; i < 200; i++) {
			if (results[i] >= 0.0 && results[i] < 2.0) {
				c = results[i] * 128.0;
				std::cout << c;	
			} else {
				std::cout << '\0';
			}
		}
		fann_train(ann, input, output);
		memmove(&input[100], input, 400);
		for (i = 0; i < 100; i++) {
			input[i] = output[i];
		}
		neuron = ann->first_layer[2].first_neuron;
		for (i = 0; i < 250; i++) {
			input[i+500] = neuron[i].value;
			input[i+750] = output[200+i];
		}
	}

	fann_save(ann, BRAIN);
}

