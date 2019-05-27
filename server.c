#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "Error: missing port\n");
		exit(EXIT_FAILURE);
	}
	int port = atoi(argv[1]);
	printf("PORT: %d", port);
	return 0;
}