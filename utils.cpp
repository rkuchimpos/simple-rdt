#include "utils.hpp"

using namespace std;

// label: "RECV" or "SEND"
void Utils::DumpPacketInfo(std::string label, Packet *pkt, int cwnd, int sstresh, bool dup_sent) {
	if (pkt == nullptr) {
		return;
	}

	cout << label;
	cout << " " << pkt->getSequenceNum() << " " << pkt->getACKNum();
	cout << " " << cwnd << " " << sstresh;
	if (pkt->isValidACK()) {
		cout << " " << "ACK";
	}
	if (pkt->getSYN()) {
		cout << " " << "SYN";
	}
	if (pkt->getFIN()) {
		cout << " " << "FIN";
	}
	if (dup_sent) {
		cout << " " << "DUP"; 
	}
	cout << endl;
}

double Utils::GetSecondsElapsed(clock_t begin, clock_t end) {
	return (double)(end - begin) / CLOCKS_PER_SEC;
}