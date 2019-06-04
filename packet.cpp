#include "packet.hpp"
#include <algorithm>
#include <iostream>

Packet::Packet() {
    payload = nullptr;
}

Packet::~Packet() {
    if (payload != nullptr) {
        // TODO: Handle pointer management
        //free(payload);
    }
}

Packet::Packet(unsigned short seq_num, unsigned short ack_num, unsigned char flags, char *payload, int payload_size) {
    hdr.SequenceNum = seq_num;
    hdr.ACKNum = ack_num;
    hdr.Flags = flags;
    memset(hdr._Gap, 0, HEADER_PADDING);
    this->payload = (char *)malloc(payload_size);
    this->payload_size = payload_size;
    memcpy(this->payload, payload, payload_size);
}

// Format packet for transmission over the network
char * Packet::AssemblePacketBuffer() {
    unsigned packet_size = std::min(HEADER_LEN + payload_size, MAX_PACKET_SIZE);
    char *packet = (char *)malloc(packet_size);
    memcpy(packet, &hdr, HEADER_LEN);
    memcpy(packet + HEADER_LEN, payload, packet_size - HEADER_LEN);
    memcpy(packet + HEADER_LEN + payload_size, &payload_size, sizeof(payload_size));

    return packet;
}

Packet Packet::CreatePacketFromBuffer(char *packet_buffer, int packet_size) {
    Packet packet = Packet();
    memcpy(&(packet.hdr), packet_buffer, HEADER_LEN);
    int payload_size = packet_size - HEADER_LEN;
    packet.payload = (char *) malloc(payload_size);
    memcpy(packet.payload, &packet_buffer[HEADER_LEN], packet_size - HEADER_LEN);
    memcpy(&(packet.payload_size), payload_size, sizeof(payload_size));

    return packet;
}

char * Packet::GetPayload() {
    return payload;
}

bool Packet::isValidACK() {
    return (bool)(hdr.Flags & FLAG_ACK);
}

bool Packet::getSYN() {
    return (bool)(hdr.Flags & FLAG_SYN);
}

bool Packet::getFIN() {
    return (bool)(hdr.Flags & FLAG_FIN);
}

unsigned short Packet::getSequenceNum() {
    return hdr.SequenceNum;
}

unsigned short Packet::getACKNum() {
    return hdr.ACKNum;
}

int Packet::GetPayloadSize() {
    return payload_size;
}