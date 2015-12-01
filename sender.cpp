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

bool isTimeout(timeval curr, timeval old) {
	timeval diff;

	timersub(&curr, &old, &diff);

	// cout << "Diff sec: " << diff.tv_sec << " Diff u_sec: " << diff.tv_usec << " ms: " << (diff.tv_usec/ 1000) << endl;

	return ((diff.tv_sec != 0) || (diff.tv_usec > (PACKET_TIMEOUT * 1000)));
}

string getCurrentTime() {
  time_t rawTime = time(0);
  string curTime(ctime(&rawTime));
  return curTime;
}

int main(int argc, char** argv) {
	int sockfd, portno, n, cwnd, end, seqNum = 0, counter = 0, pktNum = 0;
	struct sockaddr_in servAddr, clientAddr;
	socklen_t len = sizeof(clientAddr);
	string filename, line;
	char temp[100];
	Packet initial, current;
	vector<Packet> pkts;
	vector<timeval> sent_times;
	double lossThresh, corruptThresh;
	timeval curr;

	if (argc < 5) {
		cerr << "ERROR: Arguments should be of the format:" << endl;
		cerr << "sender <portnumber> <window_size> <loss_probability> <corruption_probability>" << endl;
		exit(1);
	}

	portno = atoi(argv[1]);
	cwnd = atoi(argv[2]);
	end = (cwnd / MAX_PACKET_SIZE) - 1;

	lossThresh = atof(argv[3]);
	corruptThresh = atof(argv[4]);

	if (lossThresh < 0.0 || lossThresh > 0.4 || corruptThresh < 0.0 || corruptThresh > 0.4) {
		cerr << "ERROR: Probabilities should be between 0.0 and 0.4" << endl;
		exit(1);
	}

	// Set socket and populate sender address
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	bzero((char*) &servAddr, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(portno);
	servAddr.sin_addr.s_addr = INADDR_ANY;			// Server binds to any/all interfaces

	// Bind sender to socket
	if (bind(sockfd, (struct sockaddr*) &servAddr, len) == -1) {
		cerr << "ERROR: Failed to bind socket" << endl;
		exit(1);
	}

	// Get file request
	do {
		n = recvfrom(sockfd, temp, 100, 0,
			(struct sockaddr*) &clientAddr, &len);
	} while (n == -1);

	temp[n] = '\0';
	for (int i = 0; i < strlen(temp); i++) {
		filename += temp[i];
	}

	cout << "TIMESTAMP: " << getCurrentTime() << "EVENT: " << "Requested File " << filename << endl << endl;

	ifstream request(filename.c_str(), ios::in | ios::binary);

	if (request) {
		initial = createPkt(false, seqNum, pktNum);
		initial.lastPkt = true;

		// Send ACK with initial seq number 0 confirming valid file
		sendto(sockfd, &initial, sizeof(initial), 0,
			(struct sockaddr*) &clientAddr, len);

	} else {
		initial = createPkt(false, -1, pktNum);
		initial.lastPkt = true;

		// Send ACK with initial seq number -1 confirming invalid file
		sendto(sockfd, &initial, sizeof(initial), 0,
			(struct sockaddr*) &clientAddr, len);

		cerr << "ERROR: File not found" << endl;
		exit(1);
	}

	current = createPkt(true, seqNum, pktNum);

	int dataLength = 0;

	// Split data into pkts
	while (true) {
		unsigned char c = request.get();
		if (request.eof()) {
			break;
		}
		current.data[counter] = c;
		dataLength++;

		if (counter == MAX_PACKET_SIZE - 1) {
			current.seqNum = seqNum;
			current.pktNum = pktNum;

			current.lastPkt = false;
			current.dataLength = dataLength;

			pkts.push_back(current);

			counter = -1;
			dataLength = 0;
			seqNum += MAX_PACKET_SIZE;
			pktNum++;
		}
		counter++;
	}

	if (counter != 0) {
		current.data[counter] = '\0';
		current.seqNum = seqNum;
		current.pktNum = pktNum;
		current.dataLength = dataLength;
		current.lastPkt = true;
		pkts.push_back(current);
		pktNum++;
	} else {
		pkts[pkts.size() - 1].lastPkt = true;
	}

	int base = 0;
	int next_pktNum = 0;

	// Send all initial pkts
	// cout << "Original end: " << end << endl;
	cout << "TIMESTAMP: " << getCurrentTime() << "EVENT: " << "Sending initial pkts up to window" << endl;

	for (next_pktNum; next_pktNum <= end && next_pktNum < pkts.size(); next_pktNum++) {
		cout << "TIMESTAMP: " << getCurrentTime() << "EVENT: " << "Sending packet with sequence number ";
		cout << pkts[next_pktNum].seqNum << endl; //<< " and packet number " << pkts[next_pktNum].pktNum << endl;

		sendto(sockfd, &pkts[next_pktNum], sizeof(pkts[next_pktNum]), 0,
			(struct sockaddr*) &clientAddr, len);

		gettimeofday(&curr, NULL);
		sent_times.push_back(curr);
	}

	while (true) {
		gettimeofday(&curr, NULL);
		timeval old = sent_times[0];

		// Check timeouts
		if (isTimeout(curr, old)) {
			cout << "TIMESTAMP: " << getCurrentTime() << "EVENT: " << "Timeout for packet " << base << endl;
			cout << "TIMESTAMP: " << getCurrentTime() << "EVENT: Retransmission" << endl;

			// Resend all pkts in window
			sent_times.clear();
			// cout << "Base: " << base << " Next_pktNum: " << next_pktNum << endl;
			for (int i = base; i < next_pktNum && i < pkts.size(); i++) {
				cout << "TIMESTAMP: " << getCurrentTime() << "EVENT: " << "Resending packet with sequence number ";
				cout << pkts[i].seqNum << endl; // << " and packet number " << pkts[i].pktNum << endl;

				sendto(sockfd, &pkts[i], sizeof(pkts[i]), 0,
					(struct sockaddr*) &clientAddr, len);

				gettimeofday(&curr, NULL);
				sent_times.push_back(curr);
			}
			cout << endl;
			continue;
		}

		// Get ACK
		Packet ack;
		n = recvfrom(sockfd, &ack, sizeof(ack), MSG_DONTWAIT,
			(struct sockaddr*) &clientAddr, &len);
		if (n == 0) {
			continue;	// No more messages
		}
		else if (n < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				continue;	// No messages immediately available
			}
			break;	// Error
		}

		// Packet Loss simulation
		if (isPktBad(lossThresh)) {
			cout << "TIMESTAMP: " << getCurrentTime() << "EVENT: " << "ACK " << ack.seqNum << " has been lost!" << endl << endl;
			// cout << " and packet number " << ack.pktNum << " has been lost!" << endl << endl;
			continue;
		}

		// Packet Corruption
		if (isPktBad(corruptThresh)) {
			cout << "TIMESTAMP: " << getCurrentTime() << "EVENT: " << "ACK " << ack.seqNum << " has been corrupted!" << endl << endl;
			// cout << " and packet number " << ack.pktNum << " has been corrupted!" << endl << endl;
			continue;
		}

		cout << "TIMESTAMP: " << getCurrentTime() << "EVENT: " << "Received ACK " << ack.seqNum  << endl << endl;
		//cout << " and packet number " << ack.pktNum << endl << endl;

		// All ACKS received
		if (ack.pktNum == pkts.size() - 1)
				break;

		// Slide the window upon successful cum ACK
		if (ack.pktNum >= base) {

			//cout << "Old base: " << base << endl;
			//cout << "Old end: " << end << endl;
			base = ack.pktNum + 1;
			end = base + (cwnd/MAX_PACKET_SIZE) - 1;
			//cout << "New base: " << base << endl;
			//cout << "New end: " << end << endl;

			for (next_pktNum; next_pktNum <= end && next_pktNum < pkts.size(); next_pktNum++) {
				cout << "TIMESTAMP: " << "EVENT: " << getCurrentTime() << "Sending packet with sequence number ";
				cout << pkts[next_pktNum].seqNum << endl << endl;
				// cout << " and packet number " << pkts[next_pktNum].pktNum << endl << endl;

				sendto(sockfd, &pkts[next_pktNum], sizeof(pkts[next_pktNum]), 0,
					(struct sockaddr*) &clientAddr, len);

				sent_times.erase(sent_times.begin());
				gettimeofday(&curr, NULL);
				sent_times.push_back(curr);
			}
		}

	}

}
