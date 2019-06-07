#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>

#include <cstring>
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

	// Log INTERRUPT to file
	freopen(filename, "w+", f);
	fwrite("INTERRUPT", 1, 9, f);
	fclose(f);
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
	char *file_buf = (char *) malloc(MAX_PAYLOAD_SIZE);

	// Note: No need to call listen(2); UDP is connectionless
	int file_no = 0;

	// expect receipt of this sequence num
	int expected_sequence_num = -1;

	// last successfully sent sequence num
	int current_sequence_num = -1;

	// last packet transmitted
	Packet pkt_dup = Packet(-1, -1, 0, NULL, 0);

	// generate random seed
	srand(time(NULL));

	// to prevent infinite fin loop
	bool dup_fin = false;

	while (true) {
		// bytes_rec includes header
		if (!dup_fin) {
			ssize_t bytes_rec = recvfrom(fd_sock, buf, MAX_PACKET_SIZE, 0, (struct sockaddr *)&client_addr, &client_addr_len);
		}

		if (bytes_rec > 0) {
			Packet pkt = Packet::CreatePacketFromBuffer(buf, bytes_rec);
			Utils::DumpPacketInfo("RECV", &pkt, 0, 0, false);

			// if SYN flag set (new connection)
			if (pkt.getSYN()) {
				// create and send SYNACK packet
				Packet pkt_synack = Packet(17809/*rand() % (MAX_SEQUENCE_NUM + 1)*/, pkt.getSequenceNum() + 1, FLAG_ACK | FLAG_SYN, NULL, 0);
				ssize_t bytes_sent = sendto(fd_sock, pkt_synack.AssemblePacketBuffer(), HEADER_LEN, 0, (struct sockaddr *)&client_addr, client_addr_len);
				if (bytes_sent == -1) {
					cerr << "ERROR: Unable to send packet" << endl;
				}
				Utils::DumpPacketInfo("SEND", &pkt_synack, 0, 0, false);

				expected_sequence_num = (pkt.getSequenceNum() + 1) % (MAX_SEQUENCE_NUM + 1);
				current_sequence_num = (pkt_synack.getSequenceNum()) % (MAX_SEQUENCE_NUM + 1);

				// expect an ACK to confirm establishment of connection
				// ASSUMPTION: payload
				bytes_rec = recvfrom(fd_sock, buf, MAX_PACKET_SIZE, 0, (struct sockaddr *)&client_addr, &client_addr_len);
				Packet pkt_ack = Packet::CreatePacketFromBuffer(buf, bytes_rec);
				Utils::DumpPacketInfo("RECV", &pkt_ack, 0, 0, false);

				// confirm connection
				if ((pkt_ack.isValidACK()) && (pkt_ack.getSequenceNum() == expected_sequence_num) && (pkt_ack.getACKNum() == current_sequence_num + 1)) {
					// create new file
					file_no++;
					sprintf(filename, "%d.file", file_no);
					f = fopen(filename, "w+");

					// for no payload
					current_sequence_num = (current_sequence_num + 1) % (MAX_SEQUENCE_NUM + 1);

					// for payload
					// redirect payload of packet to file buffer
					memcpy(file_buf, pkt_ack.GetPayload(), bytes_rec - HEADER_LEN);
					if (f != NULL) {
						fwrite(file_buf, 1, bytes_rec - HEADER_LEN, f);
					}

					// send ACK
					Packet pkt_send = Packet(current_sequence_num, pkt_ack.getSequenceNum() + bytes_rec - HEADER_LEN, FLAG_ACK, NULL, 0);
					ssize_t bytes_sent = sendto(fd_sock, pkt_send.AssemblePacketBuffer(), HEADER_LEN, 0, (struct sockaddr *)&client_addr, client_addr_len);
					if (bytes_sent == -1) {
						cerr << "ERROR: Unable to send packet" << endl;
					}
					Utils::DumpPacketInfo("SEND", &pkt_send, 0, 0, false);

					pkt_dup = pkt_send;
					expected_sequence_num = (pkt_ack.getSequenceNum() + bytes_rec - HEADER_LEN) % (MAX_SEQUENCE_NUM + 1);
					// current_sequence_num stays the same
				}
			} else {
				// cwnd > 1 and we lose packet, don't buffer packets after 
				if (expected_sequence_num != pkt.getSequenceNum()) {
				// retransmit packet with next expected sequence num
					ssize_t bytes_sent = sendto(fd_sock, pkt_dup.AssemblePacketBuffer(), HEADER_LEN, 0, (struct sockaddr *)&client_addr, client_addr_len);
					if (bytes_sent == -1) {
						cerr << "ERROR: Unable to send packet" << endl;
					}
					Utils::DumpPacketInfo("SEND", &pkt_dup, 0, 0, false);

					continue;
				}

				// no payload (FIN FLAG)
				if (bytes_rec - HEADER_LEN == 0) {
					// send ACK
					Packet pkt_send = Packet(current_sequence_num, pkt.getSequenceNum() + 1, FLAG_ACK, NULL, 0);
					ssize_t bytes_sent = sendto(fd_sock, pkt_send.AssemblePacketBuffer(), HEADER_LEN, 0, (struct sockaddr *)&client_addr, client_addr_len);
					if (bytes_sent == -1) {
						cerr << "ERROR: Unable to send packet" << endl;
					}
					Utils::DumpPacketInfo("SEND", &pkt_send, 0, 0, false);

					expected_sequence_num = (pkt.getSequenceNum() + 1) % (MAX_SEQUENCE_NUM + 1);
				} else { // there is payload
					// redirect payload of packet to file buffer
					memcpy(file_buf, pkt.GetPayload(), bytes_rec - HEADER_LEN);
					if (f != NULL) {
						fwrite(file_buf, 1, bytes_rec - HEADER_LEN, f);
					}

					// send ACK
					Packet pkt_send = Packet(current_sequence_num, pkt.getSequenceNum() + bytes_rec - HEADER_LEN, FLAG_ACK, NULL, 0);
					ssize_t bytes_sent = sendto(fd_sock, pkt_send.AssemblePacketBuffer(), HEADER_LEN, 0, (struct sockaddr *)&client_addr, client_addr_len);
					if (bytes_sent == -1) {
						cerr << "ERROR: Unable to send packet" << endl;
					}
					Utils::DumpPacketInfo("SEND", &pkt_send, 0, 0, false);

					pkt_dup = pkt_send;
					expected_sequence_num = (pkt.getSequenceNum() + bytes_rec - HEADER_LEN) % (MAX_SEQUENCE_NUM + 1);
					// current_sequence_num stays the same
				}
			}

			if (pkt.getFIN()) {
				dup_fin = false;

				if (f != NULL) {
					fclose(f);
				}
				Packet pkt_fin = Packet(current_sequence_num, 0, FLAG_FIN, NULL, 0);

				clock_t start_t;
				clock_t time_elapsed;

				// to force sending right away
				start_t = clock() - (0.5 * (double) CLOCKS_PER_SEC);

				// while the client connection is alive (no ACK pkt received)
				do {
					// wait for timeout before resending packet
					time_elapsed = (double) (clock() - start_t) / (double) CLOCKS_PER_SEC;
					if (time_elapsed > 0.5) {
						ssize_t bytes_sent = sendto(fd_sock, pkt_fin.AssemblePacketBuffer(), HEADER_LEN, 0, (struct sockaddr *)&client_addr, client_addr_len);
						if (bytes_sent == -1) {
							cerr << "ERROR: Unable to send packet" << endl;
						}
						Utils::DumpPacketInfo("SEND", &pkt_fin, 0, 0, false);

						start_t = clock();
					}

					ssize_t fin_bytes = recvfrom(fd_sock, buf, MAX_PACKET_SIZE, MSG_DONTWAIT, (struct sockaddr *)&client_addr, &client_addr_len);
					if (fin_bytes == -1) {
						// do nothing
					} else {
						Packet pkt_ack = Packet::CreatePacketFromBuffer(buf, fin_bytes);
						if (pkt_ack.getSequenceNum() == expected_sequence_num && pkt_ack.getACKNum() == current_sequence_num + 1) {
							Utils::DumpPacketInfo("RECV", &pkt_ack, 0, 0, false);
							break;
						} else if (pkt_ack.getFIN()) { // we were stuck in infinite loop because of lost ack to fin
							dup_fin = true;
							break;
						}
					}
				} while (true);
			}
		}
	}

	return 0;
}