#include <iostream>
#include <stdio.h>
#include <string.h>
extern "C" {
  #include "unp.h"
}

#define MSS 1024

int main () {
  fd_set readfd;
  int  sockfd, nfds;
  int servers[5]; memset(servers, -1, 5); // -1 if it's a free slot, else it's the clientSd
  unsigned short int ports[5]; memset(ports, -1, 5); // -1 if it's unassigned, else it's the port #.
  struct sockaddr_in serveraddr;

  //select
  while(true)
  {
    FD_ZERO(&readfd);
    FD_SET(fileno(stdin), &readfd);
    nfds = fileno(stdin);
    for (int i = 0; i < 5; i++) {
      if (servers[i] != -1) {
        FD_SET(servers[i], &readfd);
        nfds = nfds > servers[i] ? nfds : servers[i];
      }
    }
    Select(nfds + 1, &readfd, NULL, NULL, NULL);

    if (FD_ISSET(fileno(stdin), &readfd))
    {
      unsigned short inputnum;
      scanf("%hu", &inputnum);
      #ifdef DEBUG_
      std::cout<<"Received port #: "<<inputnum<<std::endl;
      #endif
      
      int loc;
      for (loc = 0; loc < 5; loc++) {
        if (servers[loc] == -1) break;
      }
      if (loc >= 5) break; // No connection can be made.

      sockfd = Socket(AF_INET, SOCK_STREAM, 0);

      bzero(&serveraddr, sizeof(serveraddr));
      serveraddr.sin_family = AF_INET;
      inet_aton("127.0.0.1", &(serveraddr.sin_addr));
      serveraddr.sin_port = htons(inputnum);
      ports[loc] = inputnum;
      
      Connect(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
      servers[loc] = sockfd;
      #ifdef DEBUG_
        std::cout << "Connected to port #: " << ports[loc] << std::endl;
      #endif
      
    }

    char buffer[MSS];
    for (int loc = 0; loc < 5; loc++) {
      if (FD_ISSET(servers[loc], &readfd)) {
        int n = Recv(servers[loc], buffer, MSS, 0);
        if (n > 0) {//recved something, echoing
          
          std::cout<< ports[loc] << ": " << buffer << std::endl;
          
          Send(servers[loc], buffer, MSS, 0);

        } else if (n == 0) { // Server terminated
          std::cout<< ports[loc] << ": Connection Terminated" << std::endl;
          Close(servers[loc]);
          servers[loc] = -1;
          ports[loc] = -1;
        }
      }
    }
  }



}
