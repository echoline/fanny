#include <iostream>

int
main(int argc, char **argv) {
	unsigned char c;

	while (1) {
		std::cin >> c;
		c >>= 2;
		std::cout << c;
	}
}
