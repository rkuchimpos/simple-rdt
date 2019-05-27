#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
	if (argc < 4) {
		fprintf(stderr, "Argument format: ./client <HOSTNAME-OR-IP> <PORT> <FILENAME>\n");
		exit(EXIT_FAILURE);
	}
	char *host = argv[1];
	int port = atoi(argv[2]);
	char *filename = argv[3]; 
	printf("Host: %s, Port: %d, Filename: %s\n", host, port, filename);
	return 0;
}