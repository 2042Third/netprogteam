#include <iostream>
#include <cstdlib>
#include <string>
#include <fstream>
#include <stdio.h>
extern "C" {
  #include "lib/unp.h"
}

use namespace std;

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

int MSS = 1024;

void processTftpDatagram(char* buf, int size);
void RRQ_handle(int len, char * buffer, sockaddr * clientaddr, unsigned short int TID);
void WRQ_handle(int len, char* buffer, sockaddr * clientaddr, unsigned short int TID);
bool validOpCode(uint16_t op_code) { return op_code == OP_READ || op_code == OP_WRITE || op_code == OP_ERR; }
bool canOpen(char *filename) {//check if file opens
  std::ofstream file;
  file.open(filename);
  if (file.is_open()){
    //Successfully opens file
    file.close();
    return true;
  }
  return false;
}
void alarm_handler(int signum);
void setShort(char** buffer, uint16_t op_code);

int main(int argc, char **argv)
{
	if (argc != 3)
	{
		std::cerr << "Correct usage: " << argv[0] << " [start of port range] [end of port range]" << std::endl;
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

	char * buffer[MSS];
	memset(buffer, 0, MSS);

	int n;
	unsigned short int numConnections = 0;

	while ((n = Recvfrom(listeningSocket, buffer, MSS, 0, (sockaddr *)&cliaddr, sizeof(cliaddr))) > 0)
	{
		if (numConnections >= (maxPort - startPort))
		{
			// Ran out of TIDS so break and terminate

			// Send error packet

			break;
		}
		// Got a connection, do the initialization
		short op_code = ntohs((buf[0] << 8) | buf[1]);

		if (!validOPCode(op_code)) {
			// give error .. invalid op code
		}
		if (op_code == OP_READ || op_code == OP_WRITE)
		{
			// check if we can open the file
			if (!canOpen(buffer + 2)) {
				// send error, we cannot open the file
                
                continue;
			}
		}

		unsigned short int TID = startPort + (++numConnections);

		// Fork and let the child handle the rest of that TID's tftp
		pid_t pid = Fork();
		if (pid == 0)
		{
            Close(listeningPort);
            if (op_code == OP_READ) {
                // If it's RRQ, send the first data
                RRQ_handle(len, buffer + 2, &clientaddr, TID);
            } else if (op_code == OP_WRITE) {
                // If it's WRQ, send an ack with block num = 0
                WRQ_handle(len, buffer + 2, &clientaddr, TID);
            }
		}
	}

	// Close all connections and terminate the server

	return 0;
}

void alarm_handler(int signum) {}

void setShort(char** buffer, uint16_t op_code) {
		buffer[0] = htons( op_code >> 8 );
		buffer[1] = htons( op_code );
}

void RRQ_handle(int len, char *inputBuffer, sockaddr * clientaddr, unsigned short int TID)
{

	// create a new socket
	struct sockaddr_in childaddr;
	bzero(&servaddr, sizeof(childaddr));
	childaddr.sin_family = AF_INET;
	childaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	childaddr.sin_port = htons(TID);
	int childSocket = Socket(AF_INET, SOCK_DGRAM, 0);
	Bind(childSocket, (sockaddr *) &childaddr, sizeof(childaddr));

  uint16_t blocknum = 1, ackedBlocks = 0; //block #

  if (inputBuffer[0] =='\0'){
    //buffer not working
    return;
	}

	bool response = false;
	int read_size;
	// read from the file
    struct sigaction act;
	act.sa_handler = alarm_handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGALRM, &act, NULL);

	FILE* fp;
	fp = Fopen(inputBuffer, "r");

	char buffer[516];
	char responseBuffer[516];
	while (1) {
		if (ackedBlocks == blocknum) {
			blocknum++; // The block # we are sending at this point
			if (Fgets(buffer+4, 512, fp) == NULL) {
				// EOF
				
			}
		}
		
		// write the op code
		setShort(buffer, OP_DATA);	

		// write block number
		uint16_t tmpblock = blocknum;
		buffer[2] = htons(tmpblock >> 8);
		buffer[3] = htons(tmpblock);
    
    //send the packet
    Sendto(childSocket, buffer, 516, NULL, clientaddr, sizeof(clientaddr));
    int i;
		// wait for client, timeout, 
		for (i = 0; i < 10; ++i) {
			// set 1s alarm
			read_size = -2;
      alarm(1);
			read_size = Recvfrom(childSocket, responseBuffer, 516, 0, clientaddr, sizeof(clientaddr));

			// we got a response from recvfrom
			if (read_size > -1) break;
      if (errno == EINTR) {
				// timeout occurred, so resend
				Sendto(childSocket, buffer, 516, NULL, clientaddr, sizeof(clientaddr));
      }
		}

		// if we get a response from the client ...
		if (read_size > -1) {
			// handle client data
			uint16_t op_code = 	ntohs((responseBuffer[0] << 8) | responseBuffer[1]);
			uint16_t block = 		ntohs((responseBuffer[2] << 8) | responseBuffer[3]);

			if (op_code != OP_ACK) {
				
			}

			ackedBlocks++;
		}

		if (i == 10) {
			// Send error and terminate connection
			memcpy(buffer, 0, 516);
			setShort (&buffer, OP_ERR);
			setShort (&(buffer + 2), 0);
			mempcy(buffer+4, "Terminating Connection", 23);
			Sendto(childSocket, buffer, 516, NULL, clientaddr, sizeof(clientaddr));
	
			close(childSocket);
			Fclose(fp);
		}
	}
} 

}

void WRQ_handle(int len, char* buffer, sockaddr * clientaddr, unsigned short int TID) {
    // Function handles everything related to a write request
    char* fileName = buffer;
    FILE* fp;
    fp = Fopen(buffer, "w");

    uint16_t blocknum = 0;

    // Setup the socket
    struct sockaddr_in servaddr;
		bzero(&servaddr, sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
		servaddr.sin_port = htons(TID);

    int listeningSocket = Socket(AF_INET, SOCK_DGRAM, 0);
    Bind(listeningSocket, (sockaddr *)&servaddr, sizeof(servaddr));

    char* recvBuffer[517];
    char* sendBuffer[];

    // Send the initial ack

    Sendto(listeningSocket, buff, len, 0, clientaddr, sizeof(struct sockaddr));

    while () {

    }

    Fclose(fp);
}

