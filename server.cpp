#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <iostream>
#include <string>

#define MAX_PACKET_SIZE 524
#define HEADER_LEN 12
#define MAX_SEQUENCE_NUM 25600  

void signal_handler(int signum) {
	std::cerr << "PLACEHOLDER: Received signal " << signum << std::endl;
	// TODO: Log INTERRUPT to file

	exit(0);
}

int main(int argc, char *argv[]) {
	// Parse command-line argument for port number
	if (argc < 2) {
		std::cerr << "ERROR: Missing port" << std::endl;
		exit(EXIT_FAILURE);
	}
	int port = atoi(argv[1]);
	int fd_sock;
	// Create UDP socket
	if ((fd_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		std::cerr << "ERROR: Unable to create socket" << std::endl;
		exit(EXIT_FAILURE);
	}
	struct sockaddr_in server_addr;
	struct sockaddr_in client_addr;
	socklen_t client_addr_len = sizeof(client_addr);
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(port);
	if (bind(fd_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		std::cerr << "Error: Failed to bind socket" << std::endl;
		exit(EXIT_FAILURE);
	}

	signal(SIGQUIT, signal_handler); // To test: CTRL-BACKSLASH
	signal(SIGTERM, signal_handler); // To test: Send SIGTERM via htop
	char buf[MAX_PACKET_SIZE];
	// Note: No need to call listen(2); UDP is connectionless
	while (true) {
		ssize_t n = recvfrom(fd_sock, buf, MAX_PACKET_SIZE, 0, (struct sockaddr *)&client_addr, &client_addr_len);
		if (n > 0) {
			buf[n] = 0;
			std::cout << "Received message: " << buf << std::endl;
		}	
	}

	return 0;
}