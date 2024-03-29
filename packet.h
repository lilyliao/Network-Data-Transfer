#ifndef PACKET_H
#define PACKET_H

const int MAX_PACKET_SIZE = 1024;
const int HEADER_SIZE = 8;
const int TIMEOUT = 100; // 0.1s

struct Packet {
	bool type;		// 1: message, 0: ACK
	int seqNum;
	int pktNum;
	int dataSize;
	bool lastPkt;
	unsigned char content[MAX_PACKET_SIZE];
};

/*simulate a bad packet(loss or corrupt) with the random possibility within the threshold*/
bool badPkt(double threshold) {
	double prob = rand() / (double) RAND_MAX;
	return prob <= threshold;
}

/*Create a packet with given type(ACK or msg), seq number and packet number.*/
Packet createPkt(bool type, int seq, int pktnum) {
	Packet pkt;
	pkt.type = type;
	pkt.seqNum = seq;
	pkt.pktNum = pktnum;

	return pkt;
}

#endif /* PACKET_H */
