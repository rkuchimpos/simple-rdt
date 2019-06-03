#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

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
#define MAX_FILENAME_SIZE 256

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
	memset(&client_addr, 0, sizeof(client_addr));
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
	char *file_buf = (char *)malloc(MAX_FILE_SIZE);

	// Note: No need to call listen(2); UDP is connectionless
	int file_no = 0;
	int packet_no = 0;

	// generate random seed
	srand(time(NULL));

	while (true) {
		ssize_t n = recvfrom(fd_sock, buf, MAX_PACKET_SIZE, 0, (struct sockaddr *)&client_addr, &client_addr_len);
		if (n > 0) {
			Packet p = Packet::CreatePacketFromBuffer(buf, n);

			// create general packet to send
			Packet s = Packet(p.getACKNum(), p.getSequenceNum() + n - HEADER_LEN, 1, 0, 0, NULL, 0);

			// if SYN flag set (new connection)
			if (p.getSYN()) {
				// create new file
				file_no++;
				packet_no = 0;
				char filename[MAX_FILENAME_SIZE];
				sprintf(filename, "%d.file", file_no);
				f.open(filename);

				// create SYNACK packet
				s = Packet(rand() % (MAX_SEQUENCE_NUM + 1), p.getSequenceNum() + 1, 1, 1, 0, NULL, 0);
			}

			packet_no++;

			// redirect payload of packet to file buffer
			memcpy(&file_buf[(packet_no - 1) * MAX_PAYLOAD_SIZE], p.GetPayload(), n - HEADER_LEN);

			// if FIN flag set (final connection)
			if (p.getFIN()) {
				f << file_buf;
				f.close();
			}

			// return message
			sendto(fd_sock, s.AssemblePacketBuffer(), HEADER_LEN, 0, (struct sockaddr *)&client_addr, client_addr_len);
			cout << s.getSequenceNum() << endl;
		}
	}

	return 0;
}