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
	int sockfd, portno, n;
  int curSeqNum = 0, reqPktNum = 0;
	struct sockaddr_in servAddr;
	socklen_t len = sizeof(servAddr);
  Packet msg, firstPkt, ack, lastPkt;
	vector<string> pkts;
	double lossProb, corptPro;
  string hostname, fileRequested;

	if (argc < 6) {
		cerr << "ERROR: Argument should be of the format:" << endl;
		cerr << "receiver <sender_hostname> <sender_portnumber> <fileRequested> <loss_probability> <corruption_probability>" << endl;
		exit(1);
	}

	hostname = argv[1];
	portno = atoi(argv[2]);
	fileRequested = argv[3];
	lossProb = atof(argv[4]);
	corptPro = atof(argv[5]);

	if (lossProb < 0.0 || lossProb > 1 || corptPro < 0.0 || corptPro > 1) {
		cerr << "ERROR: Probability should be between 0.0 and 1" << endl;
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
		fprintf(stderr, "Cannot find the address of %s\n", hostname.c_str());
		exit(1);
	}
	//Assign the port number to the created socket
	memset((char*) &servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	//converts the first address in the list into a network address in AF family,and copies to servAddr.sin_addr
	inet_pton(AF_INET, sender->h_addr_list[0], &(servAddr.sin_addr));
	servAddr.sin_port = htons(portno);

	// Send firstPkt request for the file
	cout << "Sending request for file: " << fileRequested.c_str() << endl << endl;
	n = sendto(sockfd, fileRequested.c_str(), fileRequested.size(), 0,
		(struct sockaddr*) &servAddr, len);

	cout << "Receiving ACK for request" << endl << endl;
	while (recvfrom(sockfd, &firstPkt, sizeof(firstPkt), 0,
		(struct sockaddr*) &servAddr, &len) == -1);

	if (firstPkt.seqNum == -1) {
		cerr << "ERROR: Cannot find file" << endl;
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
		if (isPktBad(lossProb)) {
			cout << "Packet with sequence number " << msg.seqNum << " is lost!" << endl << endl;
			continue;
		}
		// Packet Corruption
		if (isPktBad(corptPro)) {
			cout << "Packet with sequence number " << msg.seqNum << " is corrupted!" << endl << endl;
			continue;
		}
		// Packet received in order
		if (msg.pktNum == reqPktNum) {
			cout << "In order packet received with sequence number " << msg.seqNum << endl << endl;
			// get content from received packet
			string content;
			for (unsigned int i = 0; i < msg.dataSize; i++) {
				content += msg.content[i];
			}
			pkts.push_back(content);
			// create ACK
			curSeqNum += msg.dataSize;
			ack = createPkt(0, curSeqNum, reqPktNum);

			// Send ACK
			cout << "Sending ACK " << ack.seqNum << endl << endl;
			sendto(sockfd, &ack, sizeof(ack), 0,
				(struct sockaddr*) &servAddr, sizeof(servAddr));

			// add 1 to expected packet number
			reqPktNum++;
			if (msg.lastPkt) {
				break;
			}
		}
		// packet out of order
		else {
			cout << "Out of order packet received with sequence number " << msg.seqNum << endl << endl;
      //resend ack
      ack = createPkt(0, curSeqNum, reqPktNum - 1);
			cout << "Sending ACK " << ack.seqNum << endl << endl;
			sendto(sockfd, &ack, sizeof(ack), 0,
				(struct sockaddr*) &servAddr, sizeof(servAddr));
		}
	}

	// Send repeated ACK's to ensure that the sender receives the lastpkt ack
	lastPkt = createPkt(0, curSeqNum, reqPktNum - 1);
	for (int i = 0; i < 10; i++) {
		sendto(sockfd, &lastPkt, sizeof(lastPkt), 0,
			(struct sockaddr*) &servAddr, sizeof(servAddr));
	}

	// Write packet contents to file
	ofstream output;
	string outputFile = "content.out";

	output.open(outputFile.c_str(), ios::out | ios::binary);
	if (!output.is_open()) {
		cerr << "ERROR: Cannot open output file" << endl;
		exit(1);
	}
	for (int i = 0; i < pkts.size(); i++) {
		output << pkts[i];
	}
	output.close();
}
