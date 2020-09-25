#include <iostream>
#include <cstdlib>
#include <string>
#include <fstream>
#include <stdio.h>
#include <cstring>
extern "C" {
  #include "lib/unp.h"
}

void childHandler(int sigNum) {
  // Check if a child terminated, and print it out for the parent.
  Waitpid(-1, NULL, WNOHANG);
  return;
}

#define OP_READ 1
#define OP_WRITE 2
#define OP_DATA 3
#define OP_ACK 4
#define OP_ERR 5

#define RECVTIMEOUT 10

#define ERR_NOT_DEF 0
#define ERR_FILE_NOT_FOUND 1
#define ERR_PROCESS_VIOLATION 2
#define ERR_ILLEGAL_TFTP_OP 4
#define ERR_UNKNOWN_TID 5
#define ERR_FILE_ALREADY_EXISTS 6

int MSS = 1024;

void sendError (int socket, sockaddr_in client, int error_code, const char* errMsg);
void write16 (char** dst, uint16_t data);
bool fileExists(char* fileName);
void readHeader (char* recvBuffer, uint16_t *op_code, uint16_t *recv_block_num);
void RRQ_handle(int len, char * buffer, struct sockaddr_in &clientaddr, unsigned short int tid);
void WRQ_handle(int len, char * inputBuffer, struct sockaddr_in &clientaddr, unsigned short int tid);

int main(int argc, char **argv)
{

	std::cout << "???";
	if (argc != 3)
	{
		fprintf (stderr, "Correct usage: %s [start of port range] [end of port range]\n", argv[0]);
	}

	// Child termination handler
	struct sigaction act;
	act.sa_handler = childHandler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGCHLD, &act, NULL);

  unsigned short int startPort = atoi(argv[1]);
	unsigned short int maxPort = atoi(argv[2]);

	struct sockaddr_in servaddr, cliaddr;
	bzero(&servaddr, sizeof(servaddr));
	bzero(&cliaddr, sizeof(cliaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(startPort);

	int listeningSocket = Socket(AF_INET, SOCK_DGRAM, 0);
	Bind(listeningSocket, (sockaddr *)&servaddr, sizeof(servaddr));

	char buffer[MSS];
	memset(buffer, 0, MSS);

	int n;
	unsigned short int numConnections = 0;

	socklen_t len = sizeof(cliaddr);
	while ((n = Recvfrom(listeningSocket, buffer, MSS, 0, (sockaddr *)&cliaddr, &len)) > 0)
	{
		
		/*
			If we do not exceed the maximum allowed connections...
		*/
		if (numConnections >= (maxPort - startPort))
		{
			// Send error packet
			sendError(listeningSocket, cliaddr, ERR_NOT_DEF, "No TID Available");
			break; // Break and terminate server
		}
		// Got a connection, do the initialization
		short op_code = ntohs((buffer[0] << 8) | buffer[1]);


		/*
			If we do not get a Read request or a write request,
			send an error for invalid TFTP operation
		*/
		if (op_code != OP_READ && op_code != OP_WRITE) {
			// send ERR
			sendError (listeningSocket, cliaddr, ERR_ILLEGAL_TFTP_OP, "Illegal TFTP OP");
		} 
		
		/*
			If we get a Read request:
				(1) if the file doesnt exist, send an error
				(2) else, if it does exist, fork parent and initiate read request handler
		*/
		else if (op_code == OP_READ && !fileExists(buffer + 2)) {
			sendError(listeningSocket, cliaddr, ERR_FILE_NOT_FOUND, "File Not Found");
			continue;
		} 
		
		/*
			If we get a Write request:
				(1) if the file does exist, send an error
				(2) else, fork parent and initate write request
		*/
		else if (op_code == OP_WRITE && fileExists(buffer + 2)) {
			sendError(listeningSocket, cliaddr, ERR_FILE_ALREADY_EXISTS, "File Already Exists");
			continue;
		}

		unsigned short int TID = startPort + (++numConnections);

		// Fork and let the child handle the rest of that TID's tftp
		pid_t pid = Fork();
		if (pid == 0)
		{ // Child
			Close(listeningSocket);
			if (op_code == OP_READ) {
				// If it's RRQ, send the first data
				RRQ_handle(n, buffer + 2, cliaddr, TID);
			} else if (op_code == OP_WRITE) {
				// If it's WRQ, send an ack with block num = 0
				WRQ_handle(n - 2, buffer + 2, cliaddr, TID);
			}
		}
		//Parent
	}

	// Close all connections and terminate the server

	return 0;
}

void write16 (char** dst, uint16_t data) {
	// write data into the first 2 bytes of dst
	*(*dst) = ntohs((uint8_t) data >> 8);
	*(*dst+1) = ntohs ((uint8_t) data);
}

void sendError (int socket, sockaddr_in client, int error_code, const char* errMsg) {
  //       2 bytes  2 bytes        string    1 byte
  //         ----------------------------------------
  //  ERROR | 05    |  ErrorCode |   ErrMsg   |   0  |
  //         ----------------------------------------
	
	// create the client buffer
	size_t len = 4 + strlen(errMsg) + 1;
	char err_buf[len];
	char* err_buf_ = err_buf;

	memset (err_buf, 0, len);
	write16(&err_buf_, OP_ERR);
	err_buf_ += 2;
	write16(&err_buf_, error_code);
	strncpy(err_buf+4, errMsg, strlen(errMsg));

	// send the err_buf to the client
	socklen_t socklen = sizeof(client);
	Sendto(socket, err_buf, len, 0, (struct sockaddr*) &client, socklen);
}

bool fileExists (char* fileName) {
	/* Checks if file exists */
	FILE* file = fopen(fileName, "r");
	if (file) {
		fclose(file);
		return true;
	}
	return false;
}


void readHeader (char* recvBuffer, uint16_t *op_code, uint16_t *recv_block_num) {
	*op_code = ntohs((recvBuffer[0] << 8) | recvBuffer[1]);
	*recv_block_num = ntohs((recvBuffer[2] << 8) | recvBuffer[3]);
}

void RRQ_handle(int len, char * buffer, struct sockaddr_in &clientaddr, unsigned short int tid){
	uint16_t blocknum = 1;
	unsigned int track = 0;
	ssize_t recv_size;
	std::ifstream myfile;

	char recvBuffer[516];
	char sendBuffer[516];
	char* sendBuf_ = sendBuffer;

	const unsigned short int clientTid = ntohs(clientaddr.sin_port);
	// const unsigned short int serverTid = tid;
	
	struct sockaddr_in tempAddr;
	bzero(&tempAddr, sizeof(tempAddr));
	tempAddr.sin_family = AF_INET;
	tempAddr.sin_addr.s_addr = clientaddr.sin_addr.s_addr; // If this doesn't work, use INADDR_ANY
	tempAddr.sin_port = htons(clientTid);

	int mainSocket = Socket(AF_INET, SOCK_DGRAM, 0);
	Bind(mainSocket, (sockaddr *)&tempAddr, sizeof(tempAddr));

	// set DATA as the op code of the buffer
	write16(&sendBuf_, OP_DATA);

	myfile.open(buffer);
	socklen_t socklen;
	while(!myfile.eof()) {
	char* sendBuf_ = sendBuffer + 2;
		
		// set the block number.
		write16(&sendBuf_, blocknum);
		
		// write the next 512 bytes into the buffer
		// myfile.seekg(track);
		myfile.read(sendBuffer+4, 512);
		
		//try to send the buffer
		socklen = sizeof(clientaddr);
		Sendto(clientaddr.sin_port, sendBuffer, len, 0, (struct sockaddr*) &clientaddr, socklen);

		// wait for the response
		for (int i = 0; i < 10; ++i) {
			
			recv_size = -2;
			alarm(1);

      socklen = sizeof(tempAddr);
			recv_size = Recvfrom(mainSocket, recvBuffer, sizeof(recvBuffer), 0, (
				struct sockaddr*)&tempAddr, &socklen);
			
			if (recv_size == -2){//resend when timeout for 1s
				socklen = sizeof(clientaddr);
				Sendto(clientaddr.sin_port, sendBuffer, len, 0, (struct sockaddr*) &clientaddr, socklen);
			}

			if (recv_size >= 0) break;
		}

		// if the request timed out ...
		if (recv_size < 0) {
			sendError (mainSocket, tempAddr, ERR_NOT_DEF, "Timed out");
			break;
		}

		/*
			if we got a response from the client, check to make sure it is ack
			and 
		*/
		else {
			uint16_t op_code_, recv_block_num;
			readHeader (recvBuffer, &op_code_, &recv_block_num);

			// if the tid is not what we expect
			if (ntohs(tempAddr.sin_port) == clientTid) {
				sendError (mainSocket, tempAddr, ERR_UNKNOWN_TID, "Unknown Transfer ID");
			}
			else if (op_code_ != OP_ACK) {
				sendError (mainSocket, tempAddr, ERR_NOT_DEF, "Unexpected response");
			}
			else if(recv_block_num != blocknum) {
				// sendError(mainSocket, tempAddr, ERR_NOT_DEF, "Unexpected response");
			}

		}
		
		//wait for client to ACK
		track += 512;
		blocknum ++;
	}

	// cleanup
	myfile.close();
	
}

void WRQ_handle(int len, char * inputBuffer, struct sockaddr_in &clientaddr, unsigned short int tid){
	// len = the length of the input buffer
	// inputBuffer only contains 
	//  string    1 byte     string   1 byte
  //  ---------------------------------------
  // |  Filename  |   0  |    Mode    |   0  |
  //  ---------------------------------------
	uint16_t blocknum = 0;
	uint16_t nextBlocknum = 1;
	
	char recvBuffer[516]; // Buffer to recv with
	char sendBuffer[4]; // Buffer to craft and send with
	char* sendBuf_ = sendBuffer;

	const unsigned short int clientTid = ntohs(clientaddr.sin_port);
	// const unsigned short int serverTid = tid;

  // Craft Socket	
	struct sockaddr_in tempAddr;
	bzero(&tempAddr, sizeof(tempAddr));
	tempAddr.sin_family = AF_INET;
	tempAddr.sin_addr.s_addr = clientaddr.sin_addr.s_addr; // If this doesn't work, use INADDR_ANY
	tempAddr.sin_port = htons(clientTid);

	int mainSocket = Socket(AF_INET, SOCK_DGRAM, 0);
	Bind(mainSocket, (struct sockaddr *)&tempAddr, sizeof(tempAddr));

	// Open file
	FILE * writeFile = fopen(inputBuffer, "w");

	int numTimeouts = 0;
	ssize_t n;
	bool EOF_ = false;
	short op_code;
	while (!EOF_) {
		// Craft ACK
		memset(sendBuffer, 0, sizeof(sendBuffer));
		sendBuf_ = sendBuffer;
		write16(&sendBuf_, uint16_t(OP_ACK));
		sendBuf_ += 2;
		write16(&sendBuf_, uint16_t(nextBlocknum - 1));
	
	  // Send ACK
		socklen_t adrsize = sizeof(tempAddr);
		numTimeouts = 0;
		while (numTimeouts < 10) {
			Sendto(mainSocket, sendBuffer, 4, 0, (struct sockaddr*)&tempAddr, adrsize);
			alarm(1);
			n = Recvfrom(mainSocket, recvBuffer, 516, 0, (struct sockaddr*)&tempAddr, &adrsize);
			if (n >= 0) { // we got data
				break;
			}
		}
		if (numTimeouts == 10) {
			// Terminate
			sendError(mainSocket, tempAddr, ERR_NOT_DEF, "Premature termination");
			fclose(writeFile);
			Close(mainSocket);
			exit(1);
		} else {
			// Check the TID
			if (clientTid != ntohs(tempAddr.sin_port)) { // Incorrect TID
				sendError(mainSocket, tempAddr, ERR_UNKNOWN_TID, "Unknown Tranfer ID");
				continue;
			}

			op_code = ntohs((recvBuffer[0] << 8) | recvBuffer[1]);

			if (op_code != OP_DATA)
			{
				//If packet is not DATA send an error and terminate
				sendError(mainSocket, tempAddr, ERR_ILLEGAL_TFTP_OP, "Illegal TFTP OP: DATA expected");
				fclose(writeFile);
				Close(mainSocket);
				exit(1);
			} else {
				//If packet is DATA, check to see if it has the expected blocknum
				blocknum = ntohs((recvBuffer[2] << 8) | recvBuffer[3]);
				if (nextBlocknum == blocknum)
				{
					//If it is the expected blocknum write to file
					fputs(recvBuffer+4, writeFile);
				
				  if (n == 516) {
						//If data is 512 bytes, increment the nextBlockNumber?
						nextBlocknum++;
					} else {
						//If data is <512 bytes, end the loop
						EOF_ = true;
					}
				}
				else
				{
					//If it is not the expected blocknum ignore	
				}
			}
		}
	}

	//Send final ACK and Kill (maybe we need to wait?)
	memset(sendBuffer, 0, sizeof(sendBuffer));
	sendBuf_ = sendBuffer;
	write16(&sendBuf_, uint16_t(OP_ACK));
	sendBuf_ += 2;
	write16(&sendBuf_, uint16_t(blocknum));
	socklen_t socklen = sizeof(tempAddr);
	Sendto(mainSocket, sendBuffer, 4, 0, (struct sockaddr*)&tempAddr, socklen);
	
	fclose(writeFile);
	Close(mainSocket);
	exit(0);
}