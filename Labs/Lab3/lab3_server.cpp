#include <iostream>
#include <stdio.h>
#include <cstdlib>
extern "C" {
  #include "unp.h"
}

#define MSS 1024

int main (int argc, char** argv) {
  if (argc != 2) {
    std::cerr<<"Correct usage: "<<argv[0]<<" [offset from 9877 for port #]"<<std::endl;
  }
  
  // Make socket
  int sockfd = Socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in serveraddr;
  bzero(&serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons(9877 + (unsigned short) atoi(argv[1]));
  // Bind
  Bind(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
  
  //listen
  Listen(sockfd, 0);//we should support only 1
  
  //                                         v- the client's socket
  // while select ([tcpListenSocket, stdin, ...])
  //   if stdin, send msg to any connected clients.
  //      if we have an eof from the stdin, close all and terminate (after the reply?)
  //   if new connection
  int nfds;
  int clientSd = -1; // -1 if none connected, otherwise it's the connected client sd.
  fd_set readfds;
  bool isEOF = false;
  while (!isEOF) {
    
    FD_ZERO(&readfds);
    FD_SET(fileno(stdin), &readfds);
    nfds = fileno(stdin);
    if (clientSd != -1) { // Client connected, so don't check listen ???
      // Or do we ignore the connections by reading them and letting them go?
      FD_SET(clientSd, &readfds);
      nfds = nfds > clientSd ? nfds : clientSd;
      #ifdef DEBUG_
      std::cout<<"Only stdin and client now"<<std::endl;
      #endif
    } else {
      FD_SET(sockfd, &readfds);
      nfds = nfds > sockfd ? nfds : sockfd;
      #ifdef DEBUG_
      std::cout<<"Only stdin now"<<std::endl;
      #endif
    }

    Select(nfds + 1, &readfds, NULL, NULL, NULL);

    if (clientSd != -1 && FD_ISSET(clientSd, &readfds)) {
      // Read the client reply and throw it away
      char throwAwayBuff[MSS];
      int n = Recv(clientSd, &throwAwayBuff, MSS, 0);
      #ifdef DEBUG_
      std::cout<<"Accepted Echo Reply"<<std::endl;
      #endif
      if (n == 0) {
        std::cout<<"strcli: client disconnected"<<std::endl;
        clientSd = -1;
      }
    } else if (clientSd == -1 && FD_ISSET(sockfd, &readfds)) {
      // We can accept the new client connection
      #ifdef DEBUG_
      std::cout<<"Accepting connection"<<std::endl;
      #endif
      clientSd = Accept(sockfd, NULL, NULL);
      std::cout<<"Accepted connection"<<std::endl;
    }

    if (FD_ISSET(fileno(stdin), &readfds)) {
      // Read from stdin <- so we can flush it even if we don't have a client
      char input[MSS];
      
      if (fgets(input, MSS, stdin) != NULL)
      {
        if (clientSd != -1) {
          // We have a client, so send an echo
          Send(clientSd, input, strlen(input)+1, 0);
        }
      }
      
      if(feof(stdin))
      {
        isEOF = true;
        std::cout<<"Shutting down due to EOF"<<std::endl;
      }
    }
  }

  if (clientSd != -1) {
    Close(clientSd);
  }
  Close(sockfd);

  return 0;
}
