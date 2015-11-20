#ifndef PACKET_H
#define PACKET_H

const int MAX_PACKET_SIZE = 1000;
const int HEADER_SIZE = 8;
const int PACKET_TIMEOUT = 100; // 100 ms = 0.1s
const int DEFAULT = 10;

struct Packet {
	bool type;		// True for message, false for ACK
	bool last;	// True if it is last packet
	int seqNum;
	int pktNum;
	int dataLength;
	unsigned char data[MAX_PACKET_SIZE];
};

/*
	This function determines if a packet is bad by comparing a randomly
	generated probability with the threshold probability.
*/
bool isPacketBad(double threshold) {
	double prob = rand() / (double) RAND_MAX;
	return prob <= threshold;
}

/*
	This function creates a packet with given type, sequence number and
	packet number.
*/
Packet createPacket(bool msgType, int seq, int packet) {
	Packet newPkt;

	newPkt.type = msgType;
	newPkt.seqNum = seq;
	newPkt.pktNum = packet;

	return newPkt;
}

#endif /* PACKET_H */