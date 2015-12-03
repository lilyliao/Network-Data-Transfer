#include <iostream>
#include <fstream>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>

#include "packet.h"

using namespace std;

string getCurrentTime() {
  time_t rawTime = time(0);
  string curTime(ctime(&rawTime));
  return curTime;
}

int main(int argc, char** argv) {
	int sockfd, portno, n, curSeqNum = 0, reqPktNum = 0;
	struct sockaddr_in servAddr;
	socklen_t len = sizeof(servAddr);
	string hostname, filename;
	vector<string> pkts;
	Packet msg, initial, ack, lastPkt;
	double lossThresh, corruptThresh;

	if (argc < 6) {
		cerr << "ERROR: Argument should be of the format:" << endl;
		cerr << "receiver <sender_hostname> <sender_portnumber> <filename> <loss_probability> <corruption_probability>" << endl;
		exit(1);
	}

	hostname = argv[1];
	portno = atoi(argv[2]);
	filename = argv[3];
	lossThresh = atof(argv[4]);
	corruptThresh = atof(argv[5]);

	if (lossThresh < 0.0 || lossThresh > 1 || corruptThresh < 0.0 || corruptThresh > 1) {
		cerr << "ERROR: Probability should be between 0.0 and 0.4" << endl;
		exit(1);
	}

	//Create a socket
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		cerr << "ERROR: Cannot create socket" << endl;
		exit(1);
	}
	//Get the sender's address
	struct hostent *sender = gethostbyname(hostname.c_str());
	if (!sender) {
		fprintf(stderr, "could not obtain address of %s\n", hostname.c_str());
		exit(1);
	}
	//Assign the port number to the created socket
	memset((char*) &servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	//converts the first address in the list into a network address in AF family,and copies to servAddr.sin_addr
	inet_pton(AF_INET, sender->h_addr_list[0], &(servAddr.sin_addr));
	servAddr.sin_port = htons(portno);

	// Send initial request for the file
	cout << "Action: Sending initial request for file " << filename.c_str() << endl << endl;
	n = sendto(sockfd, filename.c_str(), filename.size(), 0,
		(struct sockaddr*) &servAddr, len);

	// Receive ACK from sender
	cout << "Action: " << "Receiving ACK for initial request" << endl << endl;
	while (recvfrom(sockfd, &initial, sizeof(initial), 0,
		(struct sockaddr*) &servAddr, &len) == -1);

	if (initial.seqNum == -1) {
		cerr << "ERROR: File not found" << endl;
		exit(1);
	}

	while (true) {
		memset(&msg, 0, sizeof(Packet));
		n = recvfrom(sockfd, &msg, sizeof(msg), 0,
			(struct sockaddr*) &servAddr, &len);

		if (n <= 0) {
			continue;
		}

		// Packet Loss
		if (isPktBad(lossThresh)) {
			cout << "Action: Packet with sequence number " << msg.seqNum << " has been lost!" << endl << endl;
			//cout << " and packet number " << msg.pktNum << " has been lost!" << endl << endl;
			continue;
		}

		// Packet Corruption
		if (isPktBad(corruptThresh)) {
			cout << "Action: Packet with sequence number " << msg.seqNum << " has been corrupted!" << endl << endl;
			continue;
		}

		// Packet received in order
		if (msg.pktNum == reqPktNum) {
			cout << "Action: In order packet received with sequence number " << msg.seqNum << endl << endl;

			// Extract
			string data;
			for (unsigned int i = 0; i < msg.dataLength; i++) {
				data += msg.data[i];
			}

			pkts.push_back(data);

			// Make ACK packet
			curSeqNum += msg.dataLength;
			ack = createPkt(0, curSeqNum, reqPktNum);

			// Send ACK packet
			cout << "Action: " << "Sending ACK " << ack.seqNum << endl << endl;
			// cout << " packet number " << ack.pktNum << endl << endl;
			sendto(sockfd, &ack, sizeof(ack), 0,
				(struct sockaddr*) &servAddr, sizeof(servAddr));

			// Update packet number
			reqPktNum++;

			if (msg.lastPkt) {
				// cout << "TIMESTAMP: " << getCurrentTime() << "EVENT: " << "Last packet received" << endl << endl;
				break;
			}
		}
		// Out-of-order packet
		else {
			cout << "Action: " << "Out of order packet received with sequence number " << msg.seqNum << endl << endl;
			// cout << " and packet number " << msg.pktNum << endl << endl;

			ack = createPkt(0, curSeqNum, reqPktNum - 1);

			cout << "Action: " << "Sending ACK " << ack.seqNum << endl << endl;
			// cout << " and packet number " << ack.pktNum << endl << endl;

			// Resend ACK for most recently received in-order packet
			sendto(sockfd, &ack, sizeof(ack), 0,
				(struct sockaddr*) &servAddr, sizeof(servAddr));
		}
	}

	// Send repeated ACK's to ensure that the sender does not have lastPkt ACK dropped
	// Continue to resend lastPkt packet even after client closes
	lastPkt = createPkt(0, curSeqNum, reqPktNum - 1);
	for (int i = 0; i < DEFAULT; i++) {
		sendto(sockfd, &lastPkt, sizeof(lastPkt), 0,
			(struct sockaddr*) &servAddr, sizeof(servAddr));
	}

	// Write packet contents to file
	ofstream output;
	string output_filename = "data.out";

	output.open(output_filename.c_str(), ios::out | ios::binary);
	if (!output.is_open()) {
		cerr << "ERROR: Cannot open output file" << endl;
		exit(1);
	}

	for (int i = 0; i < pkts.size(); i++) {
		//cout << pkts[i] << endl;
		output << pkts[i];
	}

	output.close();
}
