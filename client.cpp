#include <cmath>
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <cstring>
#include <string>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include "packet.hpp"
#include "utils.hpp"

#include <stack>

#define SOCKET_TIMEOUT_SEC 10
#define RTO_SEC 0.5
#define BASE_CWND 512
#define INITIAL_SSTRESH 5120
#define MAX_CWND 10240

int cwnd = BASE_CWND;
int ssthresh = INITIAL_SSTRESH;

int modulus25601(int x) {
	int mod = x % (MAX_SEQUENCE_NUM + 1);
	if (mod < 0) {
		mod += MAX_SEQUENCE_NUM + 1;
	}
	return mod;
}

void update_state(bool packet_lost) {
	if (packet_lost) {
		ssthresh = std::max(cwnd / 2, 1024);
		cwnd = BASE_CWND;
	} else {
		if (cwnd < ssthresh) { // Slow start
			cwnd += BASE_CWND;
		} else { // Congestion avoidance
			cwnd += (BASE_CWND * BASE_CWND) / cwnd;
		}
		cwnd = std::min(cwnd, MAX_CWND); 
	}
}

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

	// State information
	clock_t rto_start_time; // Retransmission timeout
	clock_t sto_start_time; // Socket timeout
	int server_seq_num = 0;
	int server_ack_num = 0;
	srand(time(NULL));
	int client_seq_num = 7859; //rand() % (MAX_SEQUENCE_NUM + 1);
	int last_ack_received = -1;
	
	char buf[MAX_PACKET_SIZE]; // Buffer for incoming data

	// Initiate connection with a three-way handshake
	Packet pkt_syn = Packet(client_seq_num, 0, FLAG_SYN, NULL, 0);
	// Send SYN packet
	ssize_t n_sent = sendto(fd_sock, pkt_syn.AssemblePacketBuffer(), HEADER_LEN, 0, (struct sockaddr *)&server_addr, server_addr_len);
	if (n_sent == -1) {
		std::cerr << "ERROR: Unable to send packet" << std::endl;
	}
	Utils::DumpPacketInfo("SEND", &pkt_syn, cwnd, ssthresh, false);
	rto_start_time = clock();
	sto_start_time = clock();
	
	// Expect SYNACK packet from server
	while (true) {
		double elapsed_sto = Utils::GetSecondsElapsed(sto_start_time, clock());
		if (elapsed_sto >= SOCKET_TIMEOUT_SEC) {
			std::cerr << "ERROR: Socket timeout" << std::endl;
			close(fd_sock);
			exit(EXIT_FAILURE);
		}
		double elapsed_rto = Utils::GetSecondsElapsed(rto_start_time, clock());
		if (elapsed_rto >= RTO_SEC) {
			// Re-send SYN packet
			ssize_t n_sent = sendto(fd_sock, pkt_syn.AssemblePacketBuffer(), HEADER_LEN, 0, (struct sockaddr *)&server_addr, server_addr_len);
			if (n_sent == -1) {
				std::cerr << "ERROR: Unable to send packet" << std::endl;
			}
			Utils::DumpPacketInfo("SEND", &pkt_syn, cwnd, ssthresh, false);
			rto_start_time = clock();
		}
		ssize_t n_recvd = recvfrom(fd_sock, buf, MAX_PACKET_SIZE, MSG_DONTWAIT, (struct sockaddr *)&server_addr, &server_addr_len);
		if (n_recvd > 0) {
			sto_start_time = clock();
			Packet pkt_synack = Packet::CreatePacketFromBuffer(buf, n_recvd);
			Utils::DumpPacketInfo("RECV", &pkt_synack, cwnd, ssthresh, pkt_synack.getACKNum() == last_ack_received);
			if (pkt_synack.isValidACK()) {
				last_ack_received = pkt_synack.getACKNum();
			}
			// Check if SYNACK packet was received
			if (pkt_synack.getSYN() && pkt_synack.isValidACK()) {
				update_state(false);
				server_seq_num = pkt_synack.getSequenceNum();
				server_ack_num = pkt_synack.getACKNum();
				break;
			}
		}
	}

	// Once we received a SYNACK packet from the server, we can respond with an ACK
	// and start transmitting the first part of the file.
	/*
	Packet pkt_final_handshake_ack = Packet(server_ack_num, server_seq_num + 1, FLAG_ACK, NULL, 0);
	n_sent = sendto(fd_sock, pkt_final_handshake_ack.AssemblePacketBuffer(), HEADER_LEN, 0, (struct sockaddr *)&server_addr, server_addr_len);
	if (n_sent == -1) {
		std::cerr << "ERROR: Unable to send packet" << std::endl;
	}
	Utils::DumpPacketInfo("SEND", &pkt_final_handshake_ack, cwnd, ssthresh, false);
	*/

	// Break up the file into byte chunks to wrap in a packet
	std::ifstream infile;
	infile.open(filename, std::ios::in | std::ios::binary);
	char payload[MAX_PAYLOAD_SIZE];

	infile.seekg(0, std::ios::end);
	int file_size = infile.tellg();

	int packets_remaining = std::ceil((double)file_size / MAX_PAYLOAD_SIZE);
	infile.seekg(0, std::ios::beg);
	int bytes_sent = 0; // Number of bytes sent successfully
	int next_seq_num = server_ack_num;
	int send_base = server_ack_num; // Sequence number of the oldest unacked byte
	bool first_payload_sent = false;
	int outgoing_packets = 0;
	bool expect_wraparound = false;
	std::stack <int> wraparound;
	bool rto_timer_running = false;
	bool lost_packet = false;
	while (packets_remaining > 0) {
		// Check for socket timeout (10 seconds)
		double elapsed_sto = Utils::GetSecondsElapsed(sto_start_time, clock());
		if (elapsed_sto >= SOCKET_TIMEOUT_SEC) {
			std::cerr << "ERROR: Socket timeout" << std::endl;
			close(fd_sock);
			exit(EXIT_FAILURE);
		}

		// Check if ACK received
		bool ack_received = false;
		ssize_t n_recvd = recvfrom(fd_sock, buf, MAX_PACKET_SIZE, MSG_DONTWAIT, (struct sockaddr *)&server_addr, &server_addr_len);
		if (n_recvd > 0) {
			sto_start_time = clock();
			Packet pkt_ack = Packet::CreatePacketFromBuffer(buf, n_recvd);
			Utils::DumpPacketInfo("RECV", &pkt_ack, cwnd, ssthresh, last_ack_received == pkt_ack.getACKNum());
			if (pkt_ack.isValidACK()) {
				last_ack_received = pkt_ack.getACKNum();
			}
			if (pkt_ack.isValidACK() && (expect_wraparound || pkt_ack.getACKNum() > send_base)) {
				int prev_bytes_sent = bytes_sent;
				bytes_sent += modulus25601((pkt_ack.getACKNum() - send_base));
				int packets_ackd = std::ceil((double)(bytes_sent - prev_bytes_sent) / MAX_PAYLOAD_SIZE);
				outgoing_packets -= packets_ackd;
				packets_remaining -= packets_ackd;
				if (pkt_ack.getACKNum() < send_base && pkt_ack.getACKNum() == wraparound.top()) {
					expect_wraparound = false;
					wraparound.pop();
				}
				send_base = pkt_ack.getACKNum();
				send_base = send_base % (MAX_SEQUENCE_NUM + 1);
				update_state(false);
				ack_received = true;
				if (outgoing_packets > 0) {
					rto_start_time = clock();
					rto_timer_running = true;
				} else {
					rto_timer_running = false;
				}
			} else {
				// Ignore packet: no operation
			}
		} else {
			// Check for retransmission timeout (0.5 seconds); i.e., packet loss
			double elapsed_rto = Utils::GetSecondsElapsed(rto_start_time, clock());
			if (!lost_packet && rto_timer_running && elapsed_rto >= RTO_SEC) { 
				// TODO: Remove debug statement
				std::cout << ">>>>>>>>>>>>>>> PACKET LOSS <<<<<<<<<<<<<<" << std::endl;
				update_state(true);
				outgoing_packets = 0;
				next_seq_num = send_base;
				next_seq_num = next_seq_num % (MAX_SEQUENCE_NUM + 1);
				infile.clear();
				infile.seekg(bytes_sent); // This will move the stream position to the smallest unacked byte
				lost_packet = true;
			}
		}

		// Send up to 2 packets for each ACK if allowed by congestion window
		if (lost_packet || ack_received || !first_payload_sent) {
			for (int i = 0; i < 2 && outgoing_packets * MAX_PAYLOAD_SIZE < cwnd; i++) {
				if (infile.peek() != EOF) {
					infile.read(payload, MAX_PAYLOAD_SIZE);
					std::streamsize payload_size = infile.gcount();
					Packet pkt = Packet(next_seq_num, first_payload_sent ? 0 : ++server_seq_num, first_payload_sent ? 0 : FLAG_ACK, payload, payload_size);
					n_sent = sendto(fd_sock, pkt.AssemblePacketBuffer(), HEADER_LEN + payload_size, 0, (struct sockaddr *)&server_addr, server_addr_len);
					if (n_sent == -1) {
						std::cerr << "ERROR: Unable to send packet" << std::endl;
					}
					Utils::DumpPacketInfo("SEND", &pkt, cwnd, ssthresh, false);
					next_seq_num += payload_size;
					next_seq_num = next_seq_num % (MAX_SEQUENCE_NUM + 1);
					if (next_seq_num < send_base) {
						expect_wraparound = true;
						wraparound.push(next_seq_num);
					}
					if (!first_payload_sent) {
						first_payload_sent = true;
					}
					outgoing_packets++;
					// Start timer after sending new data if it is not currently running
					if (!rto_timer_running) {
						rto_start_time = clock();
						rto_timer_running = true;
					}
					if (lost_packet) {
						lost_packet = false;
					}
				} else {
					break;
				}
			}
		}
	}
	infile.close();

	// Close the connection; send FIN packet
	Packet pkt_fin = Packet(send_base, 0, FLAG_FIN, NULL, 0);
	n_sent = sendto(fd_sock, pkt_fin.AssemblePacketBuffer(), HEADER_LEN, 0, (struct sockaddr *)&server_addr, server_addr_len);
	if (n_sent == -1) {
		std::cerr << "ERROR: Unable to send packet" << std::endl;
	}
	Utils::DumpPacketInfo("SEND", &pkt_fin, cwnd, ssthresh, false);
	rto_start_time = clock();
	// Expect ACK from server
	while (true) {
		double elapsed_sto = Utils::GetSecondsElapsed(sto_start_time, clock());
		if (elapsed_sto >= SOCKET_TIMEOUT_SEC) {
			std::cerr << "ERROR: Socket timeout" << std::endl;
			close(fd_sock);
			exit(EXIT_FAILURE);
		}
		double elapsed_rto = Utils::GetSecondsElapsed(rto_start_time, clock());
		if (elapsed_rto >= RTO_SEC) {
			std::cout << ">>>>>>>>>>>>>>> PACKET LOSS <<<<<<<<<<<<<<" << std::endl;
			// Re-send FIN packet on timeout
			n_sent = sendto(fd_sock, pkt_fin.AssemblePacketBuffer(), HEADER_LEN, 0, (struct sockaddr *)&server_addr, server_addr_len);
			if (n_sent == -1) {
				std::cerr << "ERROR: Unable to send packet" << std::endl;
			}
			Utils::DumpPacketInfo("SEND", &pkt_fin, cwnd, ssthresh, false);
			rto_start_time = clock();
		}
		ssize_t n_recvd = recvfrom(fd_sock, buf, MAX_PACKET_SIZE, MSG_DONTWAIT, (struct sockaddr *)&server_addr, &server_addr_len);
		if (n_recvd > 0) {
			sto_start_time = clock();
			Packet pkt_ack = Packet::CreatePacketFromBuffer(buf, n_recvd);
			Utils::DumpPacketInfo("RECV", &pkt_ack, cwnd, ssthresh, last_ack_received == pkt_ack.getACKNum());
			if (pkt_ack.isValidACK()) {
				last_ack_received = pkt_ack.getACKNum();
			}
			if (pkt_ack.isValidACK() && pkt_ack.getACKNum() == pkt_fin.getSequenceNum() + 1) {
				server_ack_num = pkt_ack.getACKNum();
				break;
			}
		}
	}
	// Wait 2 seconds for an incoming packet with FIN flag
	clock_t fin_start_time = clock();
	while (true) {
		double elapsed_sto = Utils::GetSecondsElapsed(sto_start_time, clock());
		if (elapsed_sto >= SOCKET_TIMEOUT_SEC) {
			std::cerr << "ERROR: Socket timeout" << std::endl;
			close(fd_sock);
			exit(EXIT_FAILURE);
		}
		double elapsed_fto = Utils::GetSecondsElapsed(fin_start_time, clock());
		if (elapsed_fto >= 2.0) {
			break;
		}
		ssize_t n_recvd = recvfrom(fd_sock, buf, MAX_PACKET_SIZE, MSG_DONTWAIT, (struct sockaddr *)&server_addr, &server_addr_len);
		if (n_recvd <= 0) {
			continue;
		}
		sto_start_time = clock();
		Packet pkt_fin = Packet::CreatePacketFromBuffer(buf, n_recvd);
		Utils::DumpPacketInfo("RECV", &pkt_fin, cwnd, ssthresh, false);
		// Check if FIN packet was received
		if (pkt_fin.getFIN()) {
			server_seq_num = pkt_fin.getSequenceNum();
			// Respond to FIN with an ACK packet
			Packet pkt = Packet(server_ack_num, server_seq_num + 1, FLAG_ACK, NULL, 0);
			n_sent = sendto(fd_sock, pkt.AssemblePacketBuffer(), HEADER_LEN, 0, (struct sockaddr *)&server_addr, server_addr_len);
			if (n_sent == -1) {
				std::cerr << "ERROR: Unable to send packet" << std::endl;
			}
			Utils::DumpPacketInfo("SEND", &pkt, cwnd, ssthresh, false);
		} else {
			// Drop packet: no operation
		}
	}

	close(fd_sock);
	return 0;
}
