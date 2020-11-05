// Can only use select. No fork nor threads allowed.
// ./word_guess.out [seed] [port] [dictionary_file] [longest_word_length]
// TCP and server should listen on [port]
// Dictionary filename from [dictionary_file] - 1 word per line in the file
// length of the longest word is [longest_word_length] -- note that it's the # of letters in that word
//       Note that words will not exceed 1024 bytes, and will not exceed [longest_word_length]

// Special behavior
// On server start, and game end, it chooses a random word from the dictionary, stored as the secret word.
//      - secret words are chosen using       rand() % dictionary_size
// Usernames, guess words, and secret words are not case sensitive

#include <cctype>
#include <vector>
#include <map>
#include <utility>
#include <string>
#include <set>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <sys/socket.h>

extern "C" {
  #include "unp.h"
};

#define MSS 1025
#ifdef DEBUG
const bool vDEBUG = true;
#else
const bool vDEBUG = false;
#endif

// Converts character, c, to a lowercase
// If not possible, it just returns c as a char.
char lowerC(char c);

// Returns the lowercase version of the string, s
std::string stringToLower(std::string s);

/* Comparison function on guess and secret word */
std::pair<int, int> compareGuessAndSecret(std::string Guess, std::string Secret);

/* Caseinsensitive comparison of Guess and Secret */
bool compareWord(std::string Guess, std::string Secret);

// No invariants and assertions, since it was rushed
// Used to keep track of the client socket and username mapping, with getters and setters
// Usernames are converted to lowercase and stored.
class UserDatabase {
  public:
    int numClients() {return cliSocketUserMap.size(); } // Returns the number of connected clients
    int numRegistered() {return usernames.size();} // Returns the number of registered players
    void addClient(int socket) { this->cliSocketUserMap[socket] = ""; } // Adds a new unidentified client
    void rmClient(int socket); // Removes the client (from map and set) for disconnect, and closes the socket for us
    bool Register (int socket, std::string username); // Binds a username to the client (modifies map and set) if the username is not registered. else returns false
    void disconnectAll(); // closes connection to all clients, and clears system
    void broadcast(int clientSocket, std::string msg); // Broadcasts {msg} to all clients other than {clientSocket}
    std::string getClientUsername(int socket) { return this->usernameCase[this->cliSocketUserMap[socket]]; } //returns empty string if unidentified client
    void addToFDSet(fd_set* FDSET); // adds all clients (regardles of identification and unidentification) to the fdset
    std::vector<int> getFD_ISSET(fd_set* FDSET); // Returns a list of client socket #'s that have FD_ISSET in the given FDSET
    int nfd(); // returns the largest fd out of all the clients for select
  private:
    // Assume that we cast the strings to lowercase prior to useage
    std::map<int, std::string> cliSocketUserMap; // Maps client socket # to a username. Assume that empty string is unidentified.
    std::set<std::string> usernames; // Set of taken usernames
    std::map<std::string, std::string> usernameCase;
};

void strip (char* str, size_t len, char k) {
  size_t i = len-1;
  while (i >= 0 && str[i] == k) {
    str[i] = 0;
    --i;
  }
}

int main (int argc, char** argv) {
  if (argc != 5) {
    // print the expected usage
    fprintf (stderr, "ERROR: Invalid number of arguments.\n");
    return -1;
  }

  int seed = atoi(argv[1]);
  int port = atoi(argv[2]);
  char* dictionary_file = argv[3];
  int longest_word_len = atoi(argv[4]);
  // Read the entire dictionary into a char* array. (don't sort it, read and store it in the given order)
  std::vector<std::string> dictionary;

  std::string line;
  std::ifstream file_(dictionary_file, std::ifstream::in);
  while (std::getline(file_, line)) {
    line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());
    if(line.size() <= longest_word_len) dictionary.push_back( line );
  }

  // srand([seed])
  srand(seed);
  
  // Choose a secret word
  int secret_word_number = rand() % dictionary.size();
  std::string secret_word = dictionary[secret_word_number];

  if (vDEBUG) printf ("Secret word is: %s\n", secret_word.c_str()); 

  // Create server listen (TCP) socket
  struct sockaddr_in address, clientAddr;
  bzero(&address, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	address.sin_port = htons(port);
  int addrlen;
  // bind that socket to the [port] and listen
	int server_fd = Socket(AF_INET, SOCK_STREAM, 0);
  if (bind(server_fd, (sockaddr *)&address, sizeof(address))) {
    if (vDEBUG) perror("Failed to bind\n");
    exit(-1); // kill server on error
  }
  if (listen(server_fd, 5)) {
    if (vDEBUG) perror("Failed to listen\n");
    exit(-1); // kill server on error
  }

  fd_set ReadSet;

  UserDatabase system;

  int check = 0;
  char buffer[MSS];
  int nfd;

  while (true) {
    FD_ZERO(&ReadSet);
    system.addToFDSet(&ReadSet);
    FD_SET(server_fd, &ReadSet);

    nfd = system.nfd();
    nfd = nfd > server_fd ? nfd : server_fd;
    nfd++;

    if (select(nfd, &ReadSet, NULL, NULL, NULL) == -1) {
      if (vDEBUG) perror("Select Error\n");
      exit(-1); // Kill server on error
    }
  
    if(FD_ISSET(server_fd, &ReadSet) && system.numClients() < 5)
    { 
      //    accept connection
      struct sockaddr_in trashaddr;
      bzero(&trashaddr, sizeof(trashaddr));
      socklen_t t_len = sizeof(trashaddr);
      int client_socket = accept(server_fd,(struct sockaddr*) &trashaddr, &t_len);
      if (vDEBUG) printf("Port: %d\n", ntohs(trashaddr.sin_port));
      if(client_socket == -1)
      {
        if (vDEBUG) perror("Failed to accept connection\n");
        exit(EXIT_FAILURE);
      }
      // send msg "Welcome to Guess the Word, please enter your username."
      std::string welcome_string = std::to_string(ntohs(trashaddr.sin_port)) + " Welcome to Guess the Word, please enter your username.";
      system.addClient(client_socket);
      check = send(client_socket, welcome_string.c_str(), welcome_string.length(), 0);
      if(check == -1) {
        if (vDEBUG) perror("Failed to send\n");
        exit(EXIT_FAILURE);
      }
    }

    // If clients have activity for us
    std::vector<int> activeClients = system.getFD_ISSET(&ReadSet);
    for (unsigned int i = 0; i < activeClients.size(); i++) {
      int clientSocket = activeClients[i], n;
      if ((n = recv(clientSocket, buffer, MSS, 0)) == -1) {
        // Error reading. Idk what to do.
      } else if (n == 0) { // Client disconnected
        system.rmClient(clientSocket);
      } else {
        strip(buffer, n, '\n');
        std::string guess(buffer);
        
        //if client is unidentified
        if(system.getClientUsername(clientSocket).size() == 0)
        {
            if(!system.Register(clientSocket, stringToLower(guess))){
              //send to clientSocket, "Username {msg} is already taken, please enter a different username"
              std::string taken_string = "Username " + guess + " is already taken, please enter a different username";
              check = send(clientSocket, taken_string.c_str(), taken_string.length(), 0);
              if(check == -1)
              {
                  if (vDEBUG) perror("Failed to send\n");
                  exit(EXIT_FAILURE);  
              }
            }
            else
            {
                //send to clientSocket, "Let's start playing, {username}"
                std::string play_string = "Let's start playing, " + guess;
                check = send(clientSocket, play_string.c_str(), play_string.length(), 0);
                if(check == -1)
                  exit(EXIT_FAILURE);  
                //send to clientSocket, "There are {size of the map} player(s) playing. The secret word is {secret word len} letter(s)."
                std::string playing_string("There are ");
                playing_string.append( std::to_string(system.numRegistered()) );
                playing_string.append(" player(s) playing. The secret word is ");
                playing_string.append( std::to_string(secret_word.length()) );
                playing_string.append(" letter(s).");
                check = send(clientSocket, playing_string.c_str(), playing_string.length(), 0);
                if(check == -1) {
                  if (vDEBUG) perror("Failed to bind\n");
                  exit(EXIT_FAILURE);  
                }
            }
        }
        else
        { // we got a guess
          if (guess.size() != secret_word.size()) {
            // send(clientsocket, "Invalid guess length. The secret word is 5 letter(s).")
            std::string msg = "Invalid guess length. The secret word is " + std::to_string(secret_word.size()) + " letter(s).";
            check = send(clientSocket, msg.c_str(), msg.length(), 0);
            if(check == -1)
            {
              if (vDEBUG) perror("Failed to send\n");
              exit(EXIT_FAILURE);  
            }
          } else if (compareWord(guess, secret_word)) {
            std::string msg = system.getClientUsername(clientSocket) + " has correctly guessed the word " + secret_word;
            //               system.broadcast(-1, "{Z} has correctly guessed the word {S}");
            system.broadcast(-1, msg);
            //               system.disconnectAll();
            system.disconnectAll();
            //               select new secret word 
            secret_word_number = rand() % dictionary.size();
            secret_word = dictionary[secret_word_number];
            if (vDEBUG) printf ("Secret word is: %s\n", secret_word.c_str()); 
          }
          else {
            std::pair<int, int> comparison = compareGuessAndSecret(guess, secret_word);
            std::string msg = system.getClientUsername(clientSocket) + " guessed " + guess + ": " +
                              std::to_string(comparison.first) + " letter(s) were correct and " +
                              std::to_string(comparison.second) + " letter(s) were correctly placed.";
            system.broadcast(-1, msg);
            //system.broadcast(-1, "{Current Username} guessed {Guess word}: {X} letter(s) were correct and {Y} letter(s) were to correctly placed.")
          }
        }        
      }
    }
    
  }

  close(server_fd);
}


// ================================================================================================================
// | UserDatabase Class Stuff                                                                                     |
// ================================================================================================================
// Removes the client (from map and set) for disconnect, and closes the socket for us
void UserDatabase::rmClient(int socket) {
  std::string username = this->cliSocketUserMap[socket];
  if (username != "") {
    this->usernames.erase(username);
    this->usernameCase.erase(username);
  }
  this->cliSocketUserMap.erase(socket);
  if (close(socket)) {
    exit(-1); // Kill server on error
  }
}

// Binds a username to the client (modifies map and set) if the username is not registered.
// Returns true if successful registration, false if it was already taken
bool UserDatabase::Register (int socket, std::string username_) {
  std::string username = stringToLower(username_);
  if (this->usernames.find(username) == this->usernames.end()) { // Not taken
    this->cliSocketUserMap[socket] = username;
    this->usernames.insert(username);
    this->usernameCase[username] = username_;
    return true;
  } else {
    return false;
  }
}

// closes connection to all clients, and clears system
void UserDatabase::disconnectAll() {
  for (std::map<int, std::string>::iterator i = this->cliSocketUserMap.begin(); i != this->cliSocketUserMap.end(); i++) {
    if (close(i->first)) {
      exit(-1); // Kill server on error
    }
  }
  this->cliSocketUserMap.clear();
  this->usernames.clear();
  this->usernameCase.clear();
}

// Broadcasts {msg} to all clients other than {clientSocket}
void UserDatabase::broadcast(int clientSocket, std::string msg) {
  // char * thing = (char*) malloc(msg.size() + 1 * sizeof(char));
  // memcpy(thing, msg.c_str(), msg.size()+1);

  if (vDEBUG) printf ("Broadcasting message to %ld clients.\n", this->cliSocketUserMap.size());

  for (std::map<int, std::string>::iterator i = this->cliSocketUserMap.begin(); i != this->cliSocketUserMap.end(); i++) {
    if (i->first != clientSocket) {
      if(send(i->first, msg.c_str(), msg.size(), 0) == -1) {
        if (vDEBUG) fprintf(stderr, "Failed to send to client [%s] -- %d\n", i->second.c_str(), i->first);
        exit(EXIT_FAILURE);
      }
      else {
        if (vDEBUG) printf("Successfully broadcasted to [%s]\n", i->second.c_str());
      }
    }
  }
  // free(thing);
}

// adds all connected client sockets to the fd set
void UserDatabase::addToFDSet(fd_set* FDSET) {
  for (std::map<int, std::string>::iterator i = this->cliSocketUserMap.begin(); i != this->cliSocketUserMap.end(); i++) {
    FD_SET(i->first, FDSET);
  }
}

// Returns a list of client socket #'s that have FD_ISSET in the given FDSET
std::vector<int> UserDatabase::getFD_ISSET(fd_set* FDSET) {
  std::vector<int> active;
  for (std::map<int, std::string>::iterator i = this->cliSocketUserMap.begin(); i != this->cliSocketUserMap.end(); i++) {
    if (FD_ISSET(i->first, FDSET)) {
      active.push_back(i->first);
    }
  }
  return active;
}

// returns the largest fd out of all the clients for select
int UserDatabase::nfd() {
  int nfd = -1;
  for (std::map<int, std::string>::iterator i = this->cliSocketUserMap.begin(); i != this->cliSocketUserMap.end(); i++) {
    if (i->first > nfd) {
      nfd = i->first;
    }
  }
  return nfd;
}


// ================================================================================================================
// | other Functions                                                                                              |
// ================================================================================================================
// Converts character, c, to a lowercase
// If not possible, it just returns c as a char.
char lowerC(char c) {
  return char(tolower(c));
}

// Returns the lowercase version of the string, s
std::string stringToLower(std::string s) {
  for (unsigned int i = 0; i < s.size(); i++) {
    s[i] = lowerC(s[i]);
  }
  return s;
}
/** Comparison function on guess and secret word
 * @requires Guess.size() == Secret.size()
 * @param Guess
 * @param Secret
 * @returns pair<# of correct letters, # of correctly placed letters>
 * NOTE: Secret word of "GUESS" and guess of "SNIPE" results in
 *       <2, 0> because one S in GUESS correctly matched with one S in SNIPE, and they both have an E.
 *       i.e. duplicate letters are counted as separate letters
 * Guess: spill Secret: GRUEL results in <1, 1>
 */
std::pair<int, int> compareGuessAndSecret(std::string Guess, std::string Secret) {
  int numCorrectPlace = 0;
  int numCorrectLetter = 0;
  std::map<char, int> charMapSecret;
  std::map<char, int> charMapGuess;
  // Get # of correctly placed & get character count
  for (unsigned int i = 0; i < Secret.size(); i++) {
    if (lowerC(Secret[i]) == lowerC(Guess[i])) {
      numCorrectPlace++;
    }
    // Might have to use .find and a ternary to make this cute if this is the wrong syntax
    charMapSecret[lowerC(Secret[i])]++;
    charMapGuess[lowerC(Guess[i])]++;
  }

  // Get # correct letters
  for (std::map<char, int>::iterator i = charMapSecret.begin(); i != charMapSecret.end(); i++) {
    std::map<char, int>::iterator val_;
    if ((val_ = charMapGuess.find(lowerC(i->first))) != charMapGuess.end()) { // If the guess had a char of secret
      if (i->second <= val_->second) { // Guessed >= # of this char
        numCorrectLetter += i->second;
      } else { // Guessed less than the required number of this character in secret
        numCorrectLetter += val_->second;
      }
    }
  }

  return std::pair<int, int>(numCorrectLetter, numCorrectPlace);
}

/* Caseinsensitive comparison of Guess and Secret */
bool compareWord(std::string Guess, std::string Secret) {
  if (Guess.size() != Secret.size()) {
    return false;
  }
  for (unsigned int i = 0; i < Secret.size(); i++) {
    if (lowerC(Secret[i]) != lowerC(Guess[i])) {
      return false;
    }
  }
  return true;
}
