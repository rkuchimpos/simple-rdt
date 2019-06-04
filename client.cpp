#include <iostream>
#include <fstream>
#include <string>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "packet.hpp"
#include "utils.hpp"

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
	socklen_t server_addr_len = sizeof(server_addr);
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	struct hostent *host = gethostbyname(hostname);
	if (host == NULL) {
		std::cerr << "ERROR: Unable to get host info" << std::endl;
		exit(EXIT_FAILURE);
	}
	memcpy(&server_addr.sin_addr, host->h_addr_list[0], host->h_length);
	
	char buf[MAX_PACKET_SIZE]; // buffer for incoming data
	srand(time(NULL));
	int seq_num = rand() % (MAX_SEQUENCE_NUM + 1);
	// Initiate connection with a three-way handshake
	Packet pkt_syn = Packet(seq_num, 0, FLAG_SYN, NULL, 0);
	// Send SYN packet
	ssize_t n_sent = sendto(fd_sock, pkt_syn.AssemblePacketBuffer(), HEADER_LEN, 0, (struct sockaddr *)&server_addr, server_addr_len);
	if (n_sent == -1) {
		std::cerr << "ERROR: Unable to send packet" << std::endl;
	}
	Utils::DumpPacketInfo("SEND", &pkt_syn, 0, 0, false);
	// Expect SYNACK packet from server
	Packet pkt_synack;
	while (true) {
		ssize_t n_recvd = recvfrom(fd_sock, buf, MAX_PACKET_SIZE, 0, (struct sockaddr *)&server_addr, &server_addr_len);
		pkt_synack = Packet::CreatePacketFromBuffer(buf, n_recvd);
		Utils::DumpPacketInfo("RECV", &pkt_synack, 0, 0, false);
		// Check if SYNACK packet was received
		if (pkt_synack.getSYN() && pkt_synack.isValidACK()) {
			break;
		}
	}
	// Once we received a SYNACK packet from the server, we can respond with an ACK
	// and start transmitting the first part of the file.
	
	// Break up the file into byte chunks to wrap in a packet
	std::ifstream infile;
	infile.open(filename, std::ios::in | std::ios::binary);
	char payload[MAX_PAYLOAD_SIZE];

	// Send at most 512 bytes of payload at a time
	while (infile.peek() != EOF) {
		infile.read(payload, MAX_PAYLOAD_SIZE);
		std::streamsize payload_size = infile.gcount();
		Packet pkt = Packet(seq_num, 0, FLAG_ACK, payload, payload_size);
		n_sent = sendto(fd_sock, pkt.AssemblePacketBuffer(), HEADER_LEN + payload_size, 0, (struct sockaddr *)&server_addr, server_addr_len);
		if (n_sent == -1) {
			std::cerr << "ERROR: Unable to send packet" << std::endl;
		}
		Utils::DumpPacketInfo("SEND", &pkt, 0, 0, false);
	}
	infile.close();

	return 0;
}