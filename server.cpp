#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <iostream>
#include <string>
#include <fstream>
#include "packet.hpp"

using namespace std;

#define MAX_PACKET_SIZE 524
#define MAX_PAYLOAD_SIZE 512
#define HEADER_LEN 12
#define MAX_SEQUENCE_NUM 25600
#define MAX_FILE_SIZE 100000000

ofstream f;

void signal_handler(int signum) {
	cerr << "PLACEHOLDER: Received signal " << signum << endl;

	// Log INTERRUPT to file
	f << "INTERRUPT";
	f.close();
	exit(0);
}

int main(int argc, char *argv[]) {
	// Parse command-line argument for port number
	if (argc < 2) {
		cerr << "ERROR: Missing port" << endl;
		exit(EXIT_FAILURE);
	}
	int port = atoi(argv[1]);
	int fd_sock;
	// Create UDP socket
	if ((fd_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		cerr << "ERROR: Unable to create socket" << endl;
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
		cerr << "Error: Failed to bind socket" << endl;
		exit(EXIT_FAILURE);
	}

	signal(SIGQUIT, signal_handler); // To test: CTRL-BACKSLASH
	signal(SIGTERM, signal_handler); // To test: Send SIGTERM via htop
	char buf[MAX_PACKET_SIZE];
	char file_buf[MAX_FILE_SIZE];

	// Note: No need to call listen(2); UDP is connectionless
	int file_no = 0;
	int packet_no = 0;

	while (true) {
		ssize_t n = recvfrom(fd_sock, buf, MAX_PACKET_SIZE, 0, (struct sockaddr *)&client_addr, &client_addr_len);
		if (n > 0) {
			Packet p = Packet::CreatePacketFromBuffer(buf, n);

			// if SYN flag set
			if (p.getSYN()) {
				// create new file
				file_no++;
				packet_no = 0;
				string filename;
				sprintf(filename, "%d.file", file_no);
				f.open(filename);
			}

			packet_no++;

			// redirect contents of packet to file buffer
			buf.copy(file_buf, n, (packet_no-1) * MAX_PAYLOAD_SIZE);

			// if FIN flag set
			if (p.getFIN()) {
				f << file_buf;
				f.close();
			}
		}
	}

	return 0;
}