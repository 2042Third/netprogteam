#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>

// For inet_aton
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>

#include <unistd.h>

int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "Correct usage: %s <IPAddr>\n", argv[0]);
    return -1;
  }

  struct sockaddr_in serveraddr;
  bzero(&serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_port = htons(80);

  if (inet_aton(argv[1], &(serveraddr.sin_addr)) == 0) { // convert IPv4 string to network byte
    perror("Error: ");
    return -1;
  }

  int sockfd = socket(AF_INET, SOCK_STREAM, 0); // Create socket
  int MSS = 0, randoSize, recvBufferSize = 0;

  //Get and print MSS and Receive Buffer Size before connecting
  randoSize = sizeof(MSS);
  if (getsockopt(sockfd, IPPROTO_TCP, TCP_MAXSEG, (void*) &MSS, (socklen_t *) &randoSize) != 0) {
    perror("Error getting TCP MSS: ");
    return -1;
  }

  randoSize = sizeof(recvBufferSize);
  if (getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (void*) &recvBufferSize, (socklen_t *) &randoSize) != 0) {
    perror("Error: getting socket recvBuffSize ");
    return -1;
  }

  printf("Before:\nMSS: %d bytes\nRECV Buffer Size: %d bytes\n", MSS, recvBufferSize);

  //Connect to provided IP
  if (connect(sockfd, (struct sockaddr *) &serveraddr, (socklen_t) sizeof(serveraddr)) != 0) {
    perror("Error: connecting ");
    return -1;
  }

  //Get and print MSS and Receive Buffer Size after connecting
  randoSize = sizeof(MSS);
  if (getsockopt(sockfd, IPPROTO_TCP, TCP_MAXSEG, (void*) &MSS, (socklen_t *) &randoSize) != 0) {
    perror("Error getting TCP MSS: ");
    return -1;
  }

  randoSize = sizeof(recvBufferSize);
  if (getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (void*) &recvBufferSize, (socklen_t *) &randoSize) != 0) {
    perror("Error: getting socket recvBuffSize ");
    return -1;
  }

  printf("After:\nMSS: %d bytes\nRECV Buffer Size: %d bytes\n", MSS, recvBufferSize);

  close(sockfd);

  return 0;
}

----------------------------------------------

Local (Ubuntu) run using 128.113.0.2
Before:
MSS: 536 bytes
RECV Buffer Size: 131072 bytes
After:
MSS: 1460 bytes
RECV Buffer Size: 131072 bytes

----------------------------------------------

Lab 9 Submitty Test - Google Test using 108.177.112.106
Before:
MSS: 536 bytes
RECV Buffer Size: 87380 bytes
After:
MSS: 1380 bytes
RECV Buffer Size: 374400 bytes

----------------------------------------------

Lab 9 Submitty Test - RPI Test using 128.113.0.2
Before:
MSS: 536 bytes
RECV Buffer Size: 87380 bytes
After:
MSS: 1460 bytes
RECV Buffer Size: 374400 bytes

----------------------------------------------

Lab 9 Submitty Test - localhost Test using 127.0.0.1
Before:
MSS: 536 bytes
RECV Buffer Size: 87380 bytes
After:
MSS: 21845 bytes
RECV Buffer Size: 1062000 bytes

----------------------------------------------

Explanation:
The MSS is defaulted to octets of 536 bytes before connection. They negotiate the MSS value on both ends in the initial handshake, which is why the MSS changes after we connect to the given IP in each of the test cases.
The RECV BUFFER changes after connection because their MSS changes, so they need a larger buffer to 
