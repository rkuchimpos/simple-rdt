#ifndef _UTILS_
#define _UTILS_

#include <iostream>
#include <string>
#include "packet.hpp"

class Utils {
	public:
		static void DumpPacketInfo(std::string label, Packet *pkt, int cwnd, int sstresh, bool dup_sent);
};

#endif