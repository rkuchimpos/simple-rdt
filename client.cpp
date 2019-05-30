#include <stdlib.h>
#include <iostream>
#include <string>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "packet.hpp"

int main(int argc, char *argv[]) {
	if (argc < 4) {
		std::cerr << "Argument format: ./client <HOSTNAME-OR-IP> <PORT> <FILENAME>" << std::endl;
		exit(EXIT_FAILURE);
	}
	char *hostname = argv[1];
	int port = atoi(argv[2]);
	std::string filename = argv[3]; 

	int fd_sock;
	if ((fd_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		std::cerr << "ERROR: Unable to create socket" << std::endl;
		exit(EXIT_FAILURE);
	}

	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	struct hostent *host = gethostbyname(hostname);
	if (host == NULL) {
		std::cerr << "ERROR: Unable to get host info" << std::endl;
		exit(EXIT_FAILURE);
	}
	memcpy(&server_addr.sin_addr, host->h_addr_list[0], host->h_length);
	
	// TODO: Send messages to server using sendto()
	// Test sending of initial packet
	int seq_num = rand() % (MAX_SEQUENCE_NUM + 1);
	Packet pkt0 = Packet(seq_num, 0, 0, 1, 0, NULL, 0);
	int n = sendto(fd_sock, pkt0.AssemblePacketBuffer(), HEADER_LEN, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
	if (n == -1) {
		std::cerr << "ERROR: Unable to send packet" << std::endl;
	}

	return 0;
}