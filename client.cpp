#include <iostream>
#include <string>
#include <stdlib.h>

int main(int argc, char *argv[]) {
	if (argc < 4) {
		std::cerr << "Argument format: ./client <HOSTNAME-OR-IP> <PORT> <FILENAME>" << std::endl;
		exit(EXIT_FAILURE);
	}
	std::string host = argv[1];
	int port = atoi(argv[2]);
	std::string filename = argv[3]; 
	std::cerr << "Host: " << host << ", Port: " << port << ", Filename: " << filename << std::endl;
	return 0;
}