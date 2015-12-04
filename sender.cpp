#include <iostream>
#include <fstream>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <vector>
#include <time.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/time.h>
#include <errno.h>

#include "packet.h"

using namespace std;

bool timeout(timeval curTime, timeval record) {
	timeval diff;
	timersub(&curTime, &record, &diff);
	return ((diff.tv_sec != 0) || (diff.tv_usec > (TIMEOUT * 1000)));
}

int main(int argc, char** argv) {
	int sockfd, portno, n, cwnd, end, seqNum = 0, counter = 0, pktNum = 0;
	struct sockaddr_in servAddr, clientAddr;
	socklen_t len = sizeof(clientAddr);
	string fileRequested, line;
	char buf[100];
	Packet firstPkt, cur;
	vector<Packet> pkts;
	vector<timeval> timeRecord;
	double lossProb, corptPro;
	timeval curTime;

	if (argc < 5) {
		cerr << "ERROR: Arguments should be of the format:" << endl;
		cerr << "sender <portnumber> <window_size> <loss_probability> <corruption_probability>" << endl;
		exit(1);
	}

	portno = atoi(argv[1]);
	cwnd = atoi(argv[2]);
	lossProb = atof(argv[3]);
	corptPro = atof(argv[4]);
	if (lossProb < 0.0 || lossProb > 1 || corptPro < 0.0 || corptPro > 1) {
		cerr << "ERROR: Probabilities should be between 0.0 and 1" << endl;
		exit(1);
	}
	end = (cwnd / MAX_PACKET_SIZE) - 1;

	// Set socket and populate sender address
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	bzero((char*) &servAddr, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(portno);
	servAddr.sin_addr.s_addr = INADDR_ANY;

	// Bind sender to socket
	if (bind(sockfd, (struct sockaddr*) &servAddr, len) < 0) {
		cerr << "ERROR: Failed to bind socket" << endl;
		exit(1);
	}
	// Get file request
	do {
		n = recvfrom(sockfd, buf, 100, 0, (struct sockaddr*) &clientAddr, &len);
	} while (n == -1);

	buf[n] = '\0';
	for (int i = 0; i < strlen(buf); i++) {
		fileRequested += buf[i];
	}

	cout <<"File Requested: " << fileRequested << endl << endl;
	ifstream request(fileRequested.c_str(), ios::in | ios::binary);
	if (request) {
		firstPkt = createPkt(0, seqNum, pktNum);
		firstPkt.lastPkt = true;
		// Send ACK with firstPkt seq number 0
		sendto(sockfd, &firstPkt, sizeof(firstPkt), 0,(struct sockaddr*) &clientAddr, len);
	} else {
		// invalid file
		firstPkt = createPkt(0, -1, pktNum);
		firstPkt.lastPkt = true;
		// Send ACK with firstPkt seq number -1
		sendto(sockfd, &firstPkt, sizeof(firstPkt), 0, (struct sockaddr*) &clientAddr, len);
		cerr << "ERROR: Cannot find file" << endl;
		exit(1);
	}

	cur = createPkt(1, seqNum, pktNum);
	int dataSize = 0;
	// Split content into pkts
	while (1) {
		unsigned char buf = request.get();
		if (request.eof()) {
			break;
		}
		cur.content[counter] = buf;
		dataSize++;
		//whenever it reaches the MAX_PACKET_SIZE create a new packet.
		if (counter == MAX_PACKET_SIZE - 1) {
			cur.seqNum = seqNum;
			cur.pktNum = pktNum;
			cur.lastPkt = false;
			cur.dataSize = dataSize;
			pkts.push_back(cur);
			counter = -1;
			dataSize = 0;
			seqNum += MAX_PACKET_SIZE;
			pktNum++;
		}
		counter++;
	}

	if (counter != 0) {
		cur.content[counter] = '\0';
		cur.seqNum = seqNum;
		cur.pktNum = pktNum;
		cur.dataSize = dataSize;
		cur.lastPkt = true;
		pkts.push_back(cur);
		pktNum++;
	}
	else {
		pkts[pkts.size() - 1].lastPkt = true;
	}

	int base = 0;
	int nextPkt = 0;
	// Send all firstPkt pkts
	cout << "Sending firstPkt packets to window" << endl;
	for (nextPkt; nextPkt <= end && nextPkt < pkts.size(); nextPkt++) {
		cout << "Sending packet with sequence number " << pkts[nextPkt].seqNum << endl;
		sendto(sockfd, &pkts[nextPkt], sizeof(pkts[nextPkt]), 0, (struct sockaddr*) &clientAddr, len);

		gettimeofday(&curTime, NULL);
		timeRecord.push_back(curTime);
	}

	while (1) {
		gettimeofday(&curTime, NULL);
		timeval record = timeRecord[0];
		// Check timeouts
		if (timeout(curTime, record)) {
			cout << "Timeout for packet " << base << endl;
			cout << "Retransmission" << endl;
 			int count = 0;
			// Resend all pkts in window
			timeRecord.clear();
			for (int i = base; i < nextPkt && i < pkts.size(); i++) {
				cout << "Resending packet with sequence number ";
				cout << pkts[i].seqNum << endl;

				sendto(sockfd, &pkts[i], sizeof(pkts[i]), 0,
					(struct sockaddr*) &clientAddr, len);

				gettimeofday(&curTime, NULL);
				timeRecord.push_back(curTime);
			}
			cout << endl;
			continue;
		}

		// Get ACK
		Packet ack;
		n = recvfrom(sockfd, &ack, sizeof(ack), MSG_DONTWAIT,
			(struct sockaddr*) &clientAddr, &len);
		if (n == 0) {
			continue;
		}
		else if (n < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				continue;
			}
			break;
		}
		// Packet Loss
		if (isPktBad(lossProb)) {
			cout << "ACK " << ack.seqNum << " is lost! Dropping packet" << endl << endl;
			continue;
		}
		// Packet Corruption
		if (isPktBad(corptPro)) {
			cout << "ACK " << ack.seqNum << " is corrupted! Dropping packet" << endl << endl;
			continue;
		}
		cout << "Received ACK " << ack.seqNum  << endl << endl;

		// received all acks
		if (ack.pktNum == pkts.size() - 1)
				break;

		// Slide the window if received the right ack
		if (ack.pktNum >= base) {
			base = ack.pktNum + 1;
			end = base + (cwnd/MAX_PACKET_SIZE) - 1;
			cout << "New base: " << base << endl;
			cout << "New end: " << end << endl;

			for (nextPkt; nextPkt <= end && nextPkt < pkts.size(); nextPkt++) {
				cout << "Action: Sending packet with sequence number ";
				cout << pkts[nextPkt].seqNum << endl << endl;

				sendto(sockfd, &pkts[nextPkt], sizeof(pkts[nextPkt]), 0,
					(struct sockaddr*) &clientAddr, len);

				timeRecord.erase(timeRecord.begin());
				gettimeofday(&curTime, NULL);
				timeRecord.push_back(curTime);
			}
		}
	}
}
