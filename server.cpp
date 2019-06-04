#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#include <iostream>
#include <string>
#include "packet.hpp"
#include "utils.hpp"

using namespace std;

#define MAX_FILE_SIZE 100000000
#define MAX_FILENAME_SIZE 256

FILE *f;
char filename[MAX_FILENAME_SIZE];

void signal_handler(int signum) {
	cerr << "PLACEHOLDER: Received signal " << signum << endl;

	char *iterrupt_msg = "INTERRUPT";
	// Log INTERRUPT to file
	freopen(filename, "w+", f);
	fwrite(interrupt_msg, 1, 9, f);
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
	char *file_buf = (char *)malloc(MAX_PAYLOAD_SIZE);

	// Note: No need to call listen(2); UDP is connectionless
	int file_no = 0;

	// generate random seed
	srand(time(NULL));

	while (true) {
		// bytes_rec includes header
		ssize_t bytes_rec = recvfrom(fd_sock, buf, MAX_PACKET_SIZE, 0, (struct sockaddr *)&client_addr, &client_addr_len);
		if (bytes_rec > 0) {
			Packet pkt = Packet::CreatePacketFromBuffer(buf, bytes_rec);
			Utils::DumpPacketInfo("RECV", &pkt, 0, 0, false);
			// if SYN flag set (new connection)
			if (pkt.getSYN()) {
				// create new file
				file_no++;
				sprintf(filename, "%d.file", file_no);
				f.fopen(filename, "w+");

				// create and send SYNACK packet
				Packet pkt_synack = Packet(rand() % (MAX_SEQUENCE_NUM + 1), pkt.getSequenceNum() + 1, FLAG_ACK | FLAG_SYN, NULL, 0);
				ssize_t bytes_sent = sendto(fd_sock, pkt_synack.AssemblePacketBuffer(), HEADER_LEN, 0, (struct sockaddr *)&client_addr, client_addr_len);
				if (bytes_sent == -1) {
					cerr << "ERROR: Unable to send packet" << endl;
				}
				Utils::DumpPacketInfo("SEND", &pkt_synack, 0, 0, false);
			} else {
				// A regular packet, possibly with a payload
				if (pkt.getPayloadSize() == 0) {
					// when client confirms server's SYNACK with no payload
					// send ACK
					Packet pkt_send = Packet(pkt.getACKNum(), pkt.getSequenceNum() + 1, FLAG_ACK, NULL, 0);
					ssize_t bytes_sent = sendto(fd_sock, pkt_send.AssemblePacketBuffer(), HEADER_LEN, 0, (struct sockaddr *)&client_addr, client_addr_len);
					if (bytes_sent == -1) {
						cerr << "ERROR: Unable to send packet" << endl;
					}
					Utils::DumpPacketInfo("SEND", &pkt_send, 0, 0, false);
				} else {
					// redirect payload of packet to file buffer
					memcpy(file_buf, pkt.GetPayload(), bytes_rec - HEADER_LEN);
					fwrite(file_buf, 1, bytes_rec - HEADER_LEN, f);

					// send ACK
					Packet pkt_send = Packet(pkt.getACKNum(), pkt.getSequenceNum() + bytes_rec - HEADER_LEN, FLAG_ACK, NULL, 0);
					ssize_t bytes_sent = sendto(fd_sock, pkt_send.AssemblePacketBuffer(), HEADER_LEN, 0, (struct sockaddr *)&client_addr, client_addr_len);
					if (bytes_sent == -1) {
						cerr << "ERROR: Unable to send packet" << endl;
					}
					Utils::DumpPacketInfo("SEND", &pkt_send, 0, 0, false);

					// TEST:
					cout << "Payload size: " << bytes_rec - HEADER_LEN << endl;
					char test_payload[MAX_PAYLOAD_SIZE];
					memcpy(test_payload, pkt.GetPayload(), bytes_rec - HEADER_LEN);
					test_payload[bytes_rec - HEADER_LEN] = 0;
					cout << "Test payload: " << test_payload << endl;
				}
			}

			if (pkt.getFIN()) {
				f.close();
				Packet pkt_fin = Packet(pkt.getACKNum(), 0, FLAG_FIN, NULL, 0);

				ssize_t fin_bytes;
				// while the client connection is alive (no ACK pkt received)
				do {
					ssize_t bytes_sent = sendto(fd_sock, pkt_fin.AssemblePacketBuffer(), HEADER_LEN, 0, (struct sockaddr *)&client_addr, client_addr_len);
					if (bytes_sent == -1) {
						cerr << "ERROR: Unable to send packet" << endl;
					}
					Utils::DumpPacketInfo("SEND", &pkt_send, 0, 0, false);
				} while ((fin_bytes = recvfrom(fd_sock, buf, MAX_PACKET_SIZE, 0, (struct sockaddr *)&client_addr, &client_addr_len)) < 0);
			}
		}
	}

	return 0;
}