#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#include <iostream>
#include <string>
#include <fstream>
#include "packet.hpp"
#include "utils.hpp"

using namespace std;

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
			Packet pkt = Packet::CreatePacketFromBuffer(buf, n);
			Utils::DumpPacketInfo("RECV", &pkt, 0, 0, false);
			// if SYN flag set (new connection)
			if (pkt.getSYN()) {
				// create new file
				file_no++;
				packet_no = 0;
				char filename[MAX_FILENAME_SIZE];
				sprintf(filename, "%d.file", file_no);
				f.open(filename);

				// create and send SYNACK packet
				Packet pkt_synack = Packet(rand() % (MAX_SEQUENCE_NUM + 1), pkt.getSequenceNum() + 1, FLAG_ACK | FLAG_SYN, NULL, 0);
				ssize_t n = sendto(fd_sock, pkt_synack.AssemblePacketBuffer(), HEADER_LEN, 0, (struct sockaddr *)&client_addr, client_addr_len);
				if (n == -1) {
					cerr << "ERROR: Unable to send packet" << endl;
				}
				Utils::DumpPacketInfo("SEND", &pkt_synack, 0, 0, false);
			} else if (pkt.getFIN()) {
				f << file_buf;
				f.close();
			} else {
				// A regular packet, possibly with a payload

				// redirect payload of packet to file buffer
				packet_no++;
				memcpy(&file_buf[(packet_no - 1) * MAX_PAYLOAD_SIZE], pkt.GetPayload(), n - HEADER_LEN);

				// TEST:
				cout << "Payload size: " << n - HEADER_LEN << endl;
				char test_payload[MAX_PAYLOAD_SIZE];
				memcpy(test_payload, pkt.GetPayload(), n - HEADER_LEN);
				test_payload[n - HEADER_LEN] = 0;
				cout << "Test payload: " << test_payload << endl;
			}


		}
	}

	return 0;
}