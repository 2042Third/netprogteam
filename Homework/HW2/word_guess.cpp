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
#include <map>
#include <utility>
#include <string>
#include <set>

extern "C" {
  #include "unp.h"
};

#define MSS 1025

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
class UserDatabase {
  public:
    int numClients() {return cliSocketUserMap.size(); } // Returns the number of connected clients
    void addClient(int socket) { this->cliSocketUserMap[socket] = ""; } // Adds a new unidentified client
    void rmClient(int socket); // Removes the client (from map and set) for disconnect, and closes the socket for us
    bool Register (int socket, std::string username); // Binds a username to the client (modifies map and set) if the username is not registered. else returns false
    void disconnectAll(); // closes connection to all clients, and clears system
    void broadcast(int clientSocket, std::string msg); // Broadcasts {msg} to all clients other than {clientSocket}
    std::string getClientUsername(int socket) { return this->cliSocketUserMap[socket]; } // returns empty string if unidentified client
    void addToFDSet(fd_set** FDSET); // adds all clients (regardles of identification and unidentification) to the fdset
  private:
    // Assume that we cast the strings to lowercase prior to useage
    std::map<int, std::string> cliSocketUserMap; // Maps client socket # to a username. Assume that empty string is unidentified.
    std::set<std::string> usernames; // Set of taken usernames
};

int main (int argc, char** argv) {
  if (argc != 5) {
    // print the expected usage
    return -1;
  }

  // Read the entire dictionary into a char array. (don't sort it, read and store it in the given order)

  // srand([seed])

  // Choose a secret word

  // Create server listen (TCP) socket
  // bind that socket to the [port]

  fd_set ReadSet;

  UserDatabase system;

  while (select) {
    // zero out ReadSet
    // system.addToFDSet(ReadSet)
    // FD_SET(listen tcp socket, &ReadSet);

    // if listen socket and system.numClients() < 5
    //    accept connection
    //    send msg "Welcome to Guess the Word, please enter your username."
    //    system.addClient(socket);

    // if client socket
    //    if read 0
    //        client disconnected, so system.rmClient(socket);
    //    else
    //        read the msg (remember to strip the newline if it exists)
    //        if client is unidentified (system.getClientUsername == "")
    //            if (!system.Register(socket, stringToLower(username)))
    //                send "Username {msg} is already taken, please enter a different username"
    //            else
    //                send "Let's start playing, {username}"
    //                send "There are {size of the map} player(s) playing. The secret word is {secret word len} letter(s)."
    //        else (assume the msg is the guess, and is as long as the secret word's length)
    //            system.broadcast(-1, "{Current Username} guessed {Guess word}: {X} letter(s) were correct and {Y} letter(s) were to correctly placed.");
    //            if compareWord(Guess, Secret)
    //               system.broadcast(-1, "{Z} has correctly guessed the word {S}");
    //               system.disconnectAll();
    //               select new secret word
    //            else if Guess.size() != Secret.size()
    //               send(clientsocket, "Invalid guess length. The secret word is 5 letter(s).")
    //                 
  }
}


// ================================================================================================================
// | UserDatabase Class Stuff                                                                                     |
// ================================================================================================================
// Removes the client (from map and set) for disconnect, and closes the socket for us
void UserDatabase::rmClient(int socket) {
  std::string username = this->cliSocketUserMap[socket];
  this->cliSocketUserMap.erase(socket);
  if (username != "") {
    this->usernames.erase(username);
  }
  Close(socket);
}

// Binds a username to the client (modifies map and set) if the username is not registered.
// Returns true if successful registration, false if it was already taken
bool UserDatabase::Register (int socket, std::string username) {
  if (this->usernames.find(username) == this->usernames.end()) { // Not taken
    this->cliSocketUserMap[socket] = username;
    this->usernames.insert(username);
    return true;
  } else {
    return false;
  }
}

// closes connection to all clients, and clears system
void UserDatabase::disconnectAll() {
  for (std::map<int, std::string>::iterator i = this->cliSocketUserMap.begin(); i != this->cliSocketUserMap.end(); i++) {
    Close(i->first);
  }
  this->cliSocketUserMap.clear();
  this->usernames.clear();
}

// Broadcasts {msg} to all clients other than {clientSocket}
void UserDatabase::broadcast(int clientSocket, std::string msg) {
  for (std::map<int, std::string>::iterator i = this->cliSocketUserMap.begin(); i != this->cliSocketUserMap.end(); i++) {
    if (i->first != clientSocket) {
      Send(i->first, msg.c_str(), msg.size() + 1, 0);
    }
  }
}

// adds all connected client sockets to the fd set
void UserDatabase::addToFDSet(fd_set** FDSET) {
  for (std::map<int, std::string>::iterator i = this->cliSocketUserMap.begin(); i != this->cliSocketUserMap.end(); i++) {
    FD_SET(i->first, *FDSET);
  }
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
  map<char, int> charMapSecret;
  map<char, int> charMapGuess;
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
    if (charMapGuess.find(lowerC(i->first)) != charMapGuess.end()) { // If the guess had a char of secret
      if (i->second <= charMapGuess[lowerC(i->first)].second) { // Guessed >= # of this char
        numCorrectLetter += i->second;
      } else { // Guessed less than the required number of this character in secret
        numCorrectLetter += charMapGuess[lowerC(i->first)].second;
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