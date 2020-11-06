// For I/O
#include <stdio.h>
#include <stdlib.h>

// For getaddrinfo
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

// For inet_ntop
#include <arpa/inet.h>

#include <string.h> // For memset

// Creates a copy of {addr} at the end of {list}
// Returns list
char** append(char** list, char* addr) {
  char** x = list;
  while (*x != NULL) {x++;}
  int len = x - list + 1;
  list = realloc(list, sizeof(char*) * (len + 1));
  if (list == NULL) {
    perror("Realloc failed\n");
    exit(-1);
  }
  char* temp = calloc(strlen(addr) + 1, sizeof(char));
  if (temp == NULL) {
    perror("Calloc failed\n");
    exit(-1);
  }
  memcpy(temp, addr, strlen(addr));
  list[len - 1] = temp;
  list[len] = NULL;
  return list;
}

// Frees the seen list
void freeList(char** list) {
  char** x = list;
  while (*x != NULL) {
    free(*(x++));
  }
  free(list);
}

// Returns 1 if {interest} occurs in {list}, 0 otherwise
int hasBeenSeen(char** list, char* interest) {
  while (*list != NULL) {
    if (strcmp(*(list++), interest) == 0) {
      return 1;
    }
  }
  return 0;
}

int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "Correct usage: %s <hostName>\n", argv[0]);
    return -1;
    // e.g. argv[0] www.youtube.com
  }

  char* hostName = argv[1];

  struct addrinfo hints;
  struct addrinfo* resList, *ptr;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  // hints.ai_socktype = 1; <-  Filtering duplicate results using a list instead of using hints.ai_socktype =1 (or 2) as we are unsure whether we are meant to 
  // hints.ai_socktype = 2;     limit it to just TCP or UDP.
  // hints.ai_flags = AI_PASSIVE; <- specifies that we only want addresses suitable for bind() or accept(). Not what we want?
  // hints.ai_flags = AI_ADDRCONFIG; <- might not need this. I think we want IPv4 and IPv6 regardless of whether our system can handle IPv6.

  int status;
  if ((status = getaddrinfo(hostName, NULL, &hints, &resList)) != 0) {
    printf("Interpretation: %s\n", gai_strerror(status));
    perror(gai_strerror(status));
    return -1;
  }

  char out[INET6_ADDRSTRLEN]; // INET6_ADDRSTRLEN is 46, INET_ADDRSTRLEN is 16
  char** seen = calloc(1, sizeof(char *));
  seen[0] = NULL;

  ptr = resList;
  while (ptr != NULL) {
    memset(out, 0, INET6_ADDRSTRLEN);
    // read the current value
    // IP should be ai_addr with length ai_addrlen
    if (ptr->ai_family == AF_INET) { // IPv4
      if (inet_ntop(ptr->ai_family,
                    (void*)(&(((struct sockaddr_in *)(ptr->ai_addr))->sin_addr)),
                    out, INET_ADDRSTRLEN) == NULL) {
        perror("Failed inet_ntop\n");
        freeList(seen);
        return -1;
      }
    } else { // IPv6
      if (inet_ntop(ptr->ai_family,
                    (void*)(&(((struct sockaddr_in6 *)(ptr->ai_addr))->sin6_addr)),
                    out, INET6_ADDRSTRLEN) == NULL) {
        perror("Failed inet_ntop\n");
        freeList(seen);
        return -1;
      }
    }
    
    if (!hasBeenSeen(seen, out)) {
      seen = append(seen, out);
      printf("%s\n", out);
    }

    ptr = ptr->ai_next; // Next addrinfo
  }


  // Use getaddrinfo to output al IPv4 and IPv6 addresses associated with that hostName
  freeaddrinfo(resList);
  freeList(seen);
  
  return 0;
}