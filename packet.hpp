#ifndef _PACKET_
#define _PACKET_

#define MAX_PACKET_SIZE 524
#define MAX_PAYLOAD_SIZE 512
#define HEADER_LEN 12
#define HEADER_PADDING 6
#define MAX_SEQUENCE_NUM 25600  
#define FLAG_ACK 0x1
#define FLAG_SYN 0x2
#define FLAG_FIN 0x4

struct PacketHeader {
    // Size: 12-bytes
    unsigned short SequenceNum;
    unsigned short ACKNum;
    unsigned char Flags;
    char _Gap[HEADER_PADDING]; // Ignore; used for struct padding
};

class Packet {
    private:
        struct PacketHeader hdr;
        char *payload;
        int payload_size;
    public:
        Packet();
        ~Packet();
        Packet(unsigned short seq_num, unsigned short ack_num, unsigned char flags, char *payload, int payload_size);
        Packet(const Packet &p);
        // Format packet for transmission over the network
        char * AssemblePacketBuffer();
        static Packet CreatePacketFromBuffer(char *packet_buffer, int packet_size);
        char * GetPayload();
        bool isValidACK();
        bool getSYN();
        bool getFIN();
        unsigned short getSequenceNum();
        unsigned short getACKNum();
        int GetPayloadSize();
};

#endif