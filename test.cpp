#include <iostream>
#include "packet.hpp"

int main() {
    Packet to_send = Packet(1234, 100, 0, 1, 0, "hello", 5);
    char* buffer = to_send.AssemblePacketBuffer();

    // Send...

    Packet received_packet = Packet::CreatePacketFromBuffer(buffer, 17);
    std::cout << "Payload: " << received_packet.GetPayload() << std::endl;
    return 0;
}