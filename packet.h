#ifndef PACKET_H
#define PACKET_H

const int MAX_PACKET_SIZE = 1024;
const int HEADER_SIZE = 8;
const int TIMEOUT = 100; // 0.1s

struct Packet {
	bool type;		// 1: message, 0: ACK
	bool lastPkt;
	int seqNum;
	int pktNum;
	int dataSize;
	unsigned char content[MAX_PACKET_SIZE];
};

/*simulate a bad packet(loss or corrupt) with the random possibility within the threshold*/
bool isPktBad(double threshold) {
	double prob = rand() / (double) RAND_MAX;
	return prob <= threshold;
}

/*Create a packet with given type(ACK or msg), seq number and packet number.*/
Packet createPkt(bool msgType, int seq, int packet) {
	Packet newPkt;
	newPkt.type = msgType;
	newPkt.seqNum = seq;
	newPkt.pktNum = packet;

	return newPkt;
}

#endif /* PACKET_H */
