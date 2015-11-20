#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <stdlib.h>
#include <fstream> 
#include <sstream>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>    // gethostbyname()

#include "packet.h"

using namespace std;

/*
  Returns the current time as a string to put in header 
*/
string getCurrentTime() {
  time_t raw_current_time = time(0);
  string curr_time(ctime(&raw_current_time));
  return curr_time;
}

int main(int argc, char** argv) {
	int sockfd, portno, n, curSeqNum = 0, reqPktNum = 0;
	struct sockaddr_in serv_addr;
	socklen_t len = sizeof(serv_addr);
	string hostname, filename;
	vector<string> pkts;
	Packet msg, initial, ack, last;
	double loss_threshold, corrupt_threshold;

	if (argc < 6) {
		cerr << "ERROR: Incorrect number of arguments" << endl;
		cerr << "receiver <sender_hostname> <sender_portnumber> <filename> <packet_loss_probability> <packet_corruption_probability>" << endl;
		exit(1);
	}

	hostname = argv[1];
	portno = atoi(argv[2]);
	filename = argv[3];
	loss_threshold = atof(argv[4]);
	corrupt_threshold = atof(argv[5]);

	if (loss_threshold < 0.0 || loss_threshold > 1.0 || corrupt_threshold < 0.0 || corrupt_threshold > 1.0) {
		cerr << "ERROR: Probabilities should be between 0.0 and 1.0" << endl;
		exit(1);
	}

	struct hostent *server = gethostbyname(hostname.c_str());
	if (!server) {
		cerr << "ERROR: Hostname lookup failed" << endl;
		exit(1);
	}

	// Set up socket and connection info
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		cerr << "ERROR: opening socket" << endl;
		exit(1);
	}

	bzero((char*) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	inet_pton(AF_INET, server->h_addr_list[0], &(serv_addr.sin_addr));
	serv_addr.sin_port = htons(portno);

	// Send initial request for the file
	cout << "TIMESTAMP: " << getCurrentTime() << "EVENT: " << "Sending initial request for file " << filename.c_str() << endl << endl;
	n = sendto(sockfd, filename.c_str(), filename.size(), 0,
		(struct sockaddr*) &serv_addr, len);

	// Receive ACK from server
	cout << "TIMESTAMP: " << getCurrentTime() << "EVENT: " << "Receiving ACK for initial request" << endl << endl;
	while (recvfrom(sockfd, &initial, sizeof(initial), 0,
		(struct sockaddr*) &serv_addr, &len) == -1);

	if (initial.seqNum == -1) {
		cerr << "ERROR: File not found" << endl;
		exit(1);
	}

	while (true) {
		bzero(&msg, sizeof(Packet));
		n = recvfrom(sockfd, &msg, sizeof(msg), 0,
			(struct sockaddr*) &serv_addr, &len);

		if (n <= 0) {
			continue;
		}

		// Reliability simulation
		// Packet Loss
		if (isPacketBad(loss_threshold)) {
			cout << "TIMESTAMP: " << getCurrentTime() << "EVENT: " << "Packet with sequence number " << msg.seqNum << " has been lost!" << endl << endl;
			//cout << " and packet number " << msg.pktNum << " has been lost!" << endl << endl;
			continue;
		}

		// Packet Corruption
		if (isPacketBad(corrupt_threshold)) {
			cout << "TIMESTAMP: " << getCurrentTime() << "EVENT: " << "Packet with sequence number " << msg.seqNum << " has been corrupted!" << endl << endl;
			//cout << " and packet number " << msg.pktNum << " has been corrupted!" << endl << endl;
			continue;
		}

		// Packet is received in order
		if (msg.pktNum == reqPktNum) {
			cout << "TIMESTAMP: " << getCurrentTime() << "EVENT: " << "In order packet received with sequence number " << msg.seqNum << endl << endl;
			//cout << " and packet number " << msg.pktNum << endl << endl;

			// Extract 
			string data;
			for (unsigned int i = 0; i < msg.dataLength; i++) {
				data += msg.data[i];
			}

			pkts.push_back(data);

			// Make ACK packet
			curSeqNum += msg.dataLength;
			ack = createPacket(false, curSeqNum, reqPktNum);

			// Send ACK packet
			cout << "TIMESTAMP: " << getCurrentTime() << "EVENT: " << "Sending ACK " << ack.seqNum << endl << endl;
			// cout << " and packet number " << ack.pktNum << endl << endl; 
			sendto(sockfd, &ack, sizeof(ack), 0, 
				(struct sockaddr*) &serv_addr, sizeof(serv_addr));

			// Update packet number
			reqPktNum++;

			if (msg.last) {
				// cout << "TIMESTAMP: " << getCurrentTime() << "EVENT: " << "Last packet received" << endl << endl;
				break;
			}
		}
		// Out-of-order packet
		else {
			cout << "TIMESTAMP: " << getCurrentTime() << "EVENT: " << "Out of order packet received with sequence number " << msg.seqNum << endl << endl;
			// cout << " and packet number " << msg.pktNum << endl << endl;

			ack = createPacket(false, curSeqNum, reqPktNum - 1);

			cout << "TIMESTAMP: " << getCurrentTime() << "EVENT: " << "Sending ACK " << ack.seqNum << endl << endl;
			// cout << " and packet number " << ack.pktNum << endl << endl; 

			// Resend ACK for most recently received in-order packet
			sendto(sockfd, &ack, sizeof(ack), 0, 
				(struct sockaddr*) &serv_addr, sizeof(serv_addr));
		}
	}

	// Send repeated ACK's to ensure that the server does not have last ACK dropped
	// Continue to resend last packet even after client closes
	last = createPacket(false, curSeqNum, reqPktNum - 1);
	for (int i = 0; i < DEFAULT; i++) {
		sendto(sockfd, &last, sizeof(last), 0,
			(struct sockaddr*) &serv_addr, sizeof(serv_addr));
	}

	// Write packet contents to file
	ofstream output;
	string output_filename = "a.out";

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