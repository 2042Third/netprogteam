#include <iostream>
#include <cstdlib>
#include <string>
#include <fstream>
#include <stdio.h>
#include <cstring>
extern "C" {
  #include "unp.h"
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

#define UINT16_MAX_ 65535

#ifdef DEBUG_
const bool vDEBUG = true;
#else
const bool vDEBUG = false;
#endif

int MSS = 1024;

void sendError (int socket, sockaddr_in client, int error_code, const char* errMsg);
void write16 (char* dst, uint16_t data);
bool fileExists(char* fileName);
void readHeader (char* recvBuffer, uint16_t *op_code, uint16_t *recv_block_num);
void RRQ_handle(int len, char * buffer, struct sockaddr_in &clientaddr, unsigned short tid);
void WRQ_handle(int len, char * inputBuffer, struct sockaddr_in &clientaddr, unsigned short tid);

int main(int argc, char **argv)
{

	if (vDEBUG) {
		printf ("============\n");
		printf ("|DEBUG MODE|\n");
		printf ("============\n");
		fflush(0);
	}
	if (argc != 3)
	{
		fprintf (stderr, "Correct usage: %s [start of port range] [end of port range]\n", argv[0]);
		exit(0);
	}

	// Child termination handler
	struct sigaction act;
	act.sa_handler = childHandler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGCHLD, &act, NULL);

  unsigned short startPort = (short) atoi(argv[1]);
	unsigned short maxPort = (short) atoi(argv[2]);

	unsigned short int TID = startPort;
	struct sockaddr_in servaddr, cliaddr;
	bzero(&servaddr, sizeof(servaddr));
	bzero(&cliaddr, sizeof(cliaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(TID);

	int listeningSocket = Socket(AF_INET, SOCK_DGRAM, 0);
	bind(listeningSocket, (sockaddr *)&servaddr, sizeof(servaddr));
	++TID;

	char buffer[MSS];
	memset(buffer, 0, MSS);

	int n;

	socklen_t len = sizeof(cliaddr);
	while (true)
	{

		n = recvfrom(listeningSocket, buffer, MSS, 0, (sockaddr *)&cliaddr, &len);
		if (n < 0) continue;
		
		if (vDEBUG) {
			printf ("Data size: %d\n", n);
			printf ("Buffer: [%s]\n", buffer);

			// for (int i = 0; i < n; ++i) {
			// 	printf ("[#%d:char => %c:int => %d]\n", i, *(buffer+i), (int) *(buffer+i));
			// }
		}
		/*
			If we do not exceed the maximum allowed connections...
		*/
		if (TID > maxPort)
		{
			if (vDEBUG) fprintf (stderr, "Ran out of ports.\n");
			// Send error packet
			sendError(listeningSocket, cliaddr, ERR_NOT_DEF, "No TID Available");
			break; // Break and terminate server
		}
		// Got a connection, do the initialization
		short op_code = ntohs(*((unsigned short int *)(buffer)));
		if (vDEBUG) {
			printf ("op code => %d\n", (int) op_code);
		}


		/*
			If we do not get a Read request or a write request,
			send an error for invalid TFTP operation
		*/
		if (op_code != OP_READ && op_code != OP_WRITE) {
			// send ERR
			if (vDEBUG) fprintf (stderr, "Invalid op code: %u\n", op_code);
			sendError (listeningSocket, cliaddr, ERR_ILLEGAL_TFTP_OP, "Illegal TFTP OP");
		}
		
		/*
			If we get a Read request:
				(1) if the file doesnt exist, send an error
				(2) else, if it does exist, fork parent and initiate read request handler
		*/
		else if (op_code == OP_READ && !fileExists(buffer + 2)) {
			if (vDEBUG) fprintf (stderr, "File could not be found.\n");
			sendError(listeningSocket, cliaddr, ERR_FILE_NOT_FOUND, "File Not Found");
			continue;
		} 
		
		/*
			If we get a Write request:
				(1) if the file does exist, send an error
				(2) else, fork parent and initate write request
		*/
		else if (op_code == OP_WRITE && fileExists(buffer + 2)) {
			if (vDEBUG) fprintf (stderr, "File already exists.\n");
			sendError(listeningSocket, cliaddr, ERR_FILE_ALREADY_EXISTS, "File Already Exists");
			continue;
		}

		if (vDEBUG) {
			printf ("Filename len: %ld\n", strlen(buffer + 2));
			printf ("Mode: %s\n", buffer + strlen(buffer + 2) + 3);
		}
		if (strcmp(buffer + strlen(buffer + 2) + 3, "octet") != 0) {
			if (vDEBUG) fprintf (stderr, "Mode is not binary. Terminating.\n");
			sendError(listeningSocket, cliaddr, ERR_ILLEGAL_TFTP_OP, "Not supported");
			continue;
		}

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
		else {
			++ TID;
		}
	}

	// Close all connections and terminate the server

	return 0;
}


void printFile (char* file_path) {

	FILE* fp;
	if (vDEBUG) {
		printf ("Printing file: [%s]\n", file_path);
		fflush (0);
	}
	char buf[512];
	fp = fopen(file_path, "r");
	if (fp == NULL) {
		if (vDEBUG) fprintf (stderr, "Error opening file [%s].\n", file_path);
		return;
	}
	
	while(fgets(buf, sizeof(buf), fp) != NULL) {
		printf ("%s", buf);
	}
	fflush (0);
	fclose (fp);
}

void write16 (char* dst, uint16_t data) {
	// write data into the first 2 bytes of dst
	*(dst) = (uint8_t) (data >> 8);
	*(dst+1) = (uint8_t) data;
}

void sendError (int socket, sockaddr_in client, int error_code, const char* errMsg) {
  //       2 bytes  2 bytes        string    1 byte
  //         ----------------------------------------
  //  ERROR | 05    |  ErrorCode |   ErrMsg   |   0  |
  //         ----------------------------------------
	
	// create the client buffer
	size_t len = 4 + strlen(errMsg) + 1;
	char err_buf[len];

	memset (err_buf, 0, len);
	write16(err_buf, OP_ERR);
	write16(err_buf+2, error_code);
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
	*op_code = ntohs(*((unsigned short int *)(recvBuffer)));
	*recv_block_num = (((unsigned char) recvBuffer[2]) << 8) | ((unsigned char) recvBuffer[3]);
}

void RRQ_handle(int len, char * buffer, struct sockaddr_in &clientaddr, unsigned short tid){
	uint16_t blocknum = 1;
	ssize_t recv_size;
	std::ifstream myfile;
	
	if (vDEBUG) printf ("Reading on port %u\n", tid);

	char recvBuffer[516];
	char sendBuffer[516];

	const unsigned short int clientTid = ntohs(clientaddr.sin_port);
	// const unsigned short int serverTid = tid;
	
	struct sockaddr_in tempAddr;
	bzero(&tempAddr, sizeof(tempAddr));
	tempAddr.sin_family = AF_INET;
	tempAddr.sin_addr.s_addr = htonl(INADDR_ANY); // If this doesn't work, use INADDR_ANY
	tempAddr.sin_port = htons(tid);

	int mainSocket = Socket(AF_INET, SOCK_DGRAM, 0);
	Bind(mainSocket, (sockaddr *)&tempAddr, sizeof(tempAddr));

	// set DATA as the op code of the buffer
	write16(sendBuffer, OP_DATA);

	FILE * readFile = fopen(buffer, "r");
	if (readFile == NULL) {
		if (vDEBUG) fprintf (stderr, "Error opening file...\n");
		return;
	}

	socklen_t socklen;
	size_t datalen;
	while (true) {
		
		// set the block number.
		write16(sendBuffer+2, blocknum);
		memset(sendBuffer+4, 0, 512);
		datalen = fread(sendBuffer+4, sizeof(char), 512, readFile);
		if (vDEBUG) printf("Read %ld bytes from file.\n", datalen);

		//try to send the buffer
		socklen = sizeof(clientaddr);
		Sendto(mainSocket, sendBuffer, datalen+4, 0, (struct sockaddr*) &clientaddr, socklen);

		// wait for the response
		for (int i = 0; i < 10; ++i) {
			
			recv_size = -2;
			
      socklen = sizeof(tempAddr);
			alarm(1);
			recv_size = Recvfrom(mainSocket, recvBuffer, sizeof(recvBuffer), 0, (
				struct sockaddr*)&clientaddr, &socklen);
			
			if (recv_size == -2){//resend when timeout for 1s
				socklen = sizeof(clientaddr);
				Sendto(mainSocket, sendBuffer, len, 0, (struct sockaddr*) &clientaddr, socklen);
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
		++blocknum;
	}

	// cleanup
	fclose(readFile);
	
}

void WRQ_handle(int len, char * inputBuffer, struct sockaddr_in &clientaddr, unsigned short tid){
	// len = the length of the input buffer
	// inputBuffer only contains 
	//  string    1 byte     string   1 byte
  //  ---------------------------------------
  // |  Filename  |   0  |    Mode    |   0  |
  //  ---------------------------------------
	uint16_t blocknum = 1;

	if (vDEBUG) printf ("Writing on port %u\n", tid);
	
	char recvBuffer[516]; // Buffer to recv with
	char sendBuffer[4]; // Buffer to craft and send with

	const unsigned short clientTid = ntohs(clientaddr.sin_port);
	// const unsigned short int serverTid = tid;

  // Craft Socket	
	struct sockaddr_in childServerAddr;
	bzero(&childServerAddr, sizeof(childServerAddr));
	childServerAddr.sin_family = AF_INET;
	childServerAddr.sin_addr.s_addr = htonl(INADDR_ANY); // If this doesn't work, use INADDR_ANY
	childServerAddr.sin_port = htons(tid);

	int mainSocket = Socket(AF_INET, SOCK_DGRAM, 0);
	Bind(mainSocket, (struct sockaddr *)&childServerAddr, sizeof(childServerAddr));

	// Open file
	FILE * writeFile = fopen(inputBuffer, "w");

	int i, n;
	bool EOF_ = false;
	uint16_t op_code;
	uint32_t bytes_written = 0;

	// send initial ACK
	socklen_t adrsize = sizeof(childServerAddr);
	{
		write16(sendBuffer, (uint16_t) OP_ACK);
		write16(sendBuffer+2, (uint16_t) 0);
		Sendto(mainSocket, sendBuffer, 4, 0, (struct sockaddr*)&clientaddr, adrsize);
	}

	while (!EOF_) {

		// wait for next block of data
		adrsize = sizeof(childServerAddr);
		memset(recvBuffer, 0, 516);
		for (i = 0; i < 10; ++i) {

			// if we timeout, resend ack
			if (i != 0) {
				write16(sendBuffer, (uint16_t) OP_ACK);
				write16(sendBuffer+2, (uint16_t) blocknum-1);
				Sendto(mainSocket, sendBuffer, 4, 0, (struct sockaddr*)&clientaddr, adrsize);
			}

			n = -2;
			alarm (1);
			n = Recvfrom(mainSocket, recvBuffer, 516, 0, (struct sockaddr*)&clientaddr, &adrsize);
			if (n >= 0) break;
		}

		// If we timeout ...
		if (n == -2 || n == -1) {
			fprintf (stderr, "Client timed out... (TODO)\n");
			if (n == -2) sendError(mainSocket, childServerAddr, ERR_NOT_DEF, "Premature termination");
			if (n == -1) sendError(mainSocket, childServerAddr, ERR_NOT_DEF, "Premature termination");
			fclose(writeFile);
			Close(mainSocket);
			exit(1);
		}

		/*
		We recieved data from the client.
		*/
		else {
			if (vDEBUG) printf ("Client sent %d bytes after %d timeouts\n", n, i);

			/*
			verify that the data is correct:
				(1) block is correct
				(2) TID is correct
			*/
			op_code = ntohs(*((unsigned short int *)(recvBuffer)));
			uint16_t data_block_num = ((unsigned char) recvBuffer[3]) | (( (unsigned char) recvBuffer[2]) << 8);

			if (vDEBUG) {
				printf ("OP code: %s\n", op_code == OP_DATA ? "IS_DATA": "NOT_DATA");
				fflush(0);
			}

			// incorrect tld ...
			if (clientTid != ntohs(clientaddr.sin_port)) { // Incorrect TID
				if (vDEBUG) {
					fprintf (stderr, "TIDs do not match... Sending error.\n");
					fflush(0);
				}
				sendError(mainSocket, childServerAddr, ERR_UNKNOWN_TID, "Unknown Tranfer ID");
				continue;
			}
			else if (op_code != OP_DATA) {
				// ... ignore for now
				if (vDEBUG) {
					fprintf (stderr, "OP code is not DATA\n");
					fflush(0);
				}

				// TODO send error
				sendError(mainSocket, childServerAddr, ERR_NOT_DEF, "Unexpected response: OP code is not DATA");
				continue;
			}

			// max block value acquired...
			else if (data_block_num == UINT16_MAX_) {
				if (vDEBUG) fprintf (stderr, "Maximum block value recieved. Cannot read any more.\n");

				// fputs(recvBuffer+4, writeFile);
				fwrite(recvBuffer + 4, sizeof(char), n-4, writeFile);
				bytes_written += n-4;
				if (vDEBUG) printf ("Bytes Written: %d\n", bytes_written);
				// send final ACK
				write16(sendBuffer, (uint16_t) OP_ACK);
				write16(sendBuffer+2, (uint16_t) blocknum);
				Sendto(mainSocket, sendBuffer, 4, 0, (struct sockaddr*)&clientaddr, adrsize);

				fclose(writeFile);
				writeFile = NULL;
				if (false) printFile (inputBuffer);

				Close(mainSocket);
				mainSocket = -1;
				return;
			}

			else if (data_block_num == blocknum)  {
				if (vDEBUG) {
					printf ("Recieved correct block [%d]\n", blocknum);
					fflush(0);
				}
			
				// write the data to our file.
				if (n < 516) recvBuffer[n] = 0;
				// fputs(recvBuffer+4, writeFile);
				fwrite(recvBuffer + 4, sizeof(char), n-4, writeFile);
				bytes_written += n-4;
				if (vDEBUG) printf ("Bytes Written: %d\n", bytes_written);

				// send the ACK
				write16(sendBuffer, (uint16_t) OP_ACK);
				write16(sendBuffer+2, (uint16_t) blocknum);
				Sendto(mainSocket, sendBuffer, 4, 0, (struct sockaddr*)&clientaddr, adrsize);
				++blocknum;

				// if this is the last packet
				if (n < 516) {
					if (vDEBUG) printf("Finished sending file!\n");

					fclose(writeFile);
					writeFile = NULL;
					if (false) printFile (inputBuffer);

					Close(mainSocket);
					mainSocket = -1;
					break;
				}
			}
			else {
				// ... not the block we expect ...
				if (vDEBUG) {
					fprintf (stderr, "Unmet case...\n");
				}
				continue;
			}
		}
	}
	
	if (writeFile != NULL) fclose(writeFile);
	if (mainSocket != -1) Close(mainSocket);
	exit(0);
}