#include <iostream>
#include <string>
#include <stdlib.h>

int main(int argc, char *argv[]) {
	if (argc < 2) {
		std::cerr << "Error: missing port" << std::endl;
		exit(EXIT_FAILURE);
	}
	int port = atoi(argv[1]);
	std::cerr << "PORT: " << port << std::endl;
	return 0;
}