#ifndef _PACKET_
#define _PACKET_

#define MAX_PACKET_SIZE 524
#define HEADER_LEN 12
#define MAX_SEQUENCE_NUM 25600  

struct PacketHeader {
    // Size: 12-bytes
    unsigned short SequenceNum;
    unsigned short ACKNum;
    unsigned char FlagACK;
    unsigned char FlagSYN;
    unsigned char FlagFIN;
    char _Gap[4]; // Ignore; used for struct padding
};

class Packet {
    private:
        struct PacketHeader hdr;
        char *payload;
        int payload_size;
    public:
        Packet();
        ~Packet();
        Packet(unsigned short seq_num, unsigned short ack_num, unsigned char flag_ack, unsigned char flag_syn, unsigned char flag_fin, char *payload, int payload_size);
        // Format packet for transmission over the network
        char * AssemblePacketBuffer();
        static Packet CreatePacketFromBuffer(char *packet_buffer, int packet_size);
        char * GetPayload();
        bool isValidACK();
        bool getSYN();
        bool getFIN();
};

#endif