// Notes
// No bookcode is available. Simple pthread should be used.

// Use threads similar to fib_thread.c, to add every combination of numbers [1...(MAX_ADDAND-1)] to every combination of numbers [1...MAX_ADDAND]
// using add(),
// then output results after creating all computation threads.
// Need to pthread_join and print the results in the order the threads were made


/* Structure to pass into the threads */
struct Computation {
  int a;
  int b;
  int sum;
};

/** Thread function to recursively add a + b
 * @param a is any integer
 * @param b is any non-negative integer
 * @returns a + b
 */
int add(int a, int b) {
  // Print "Thread {pthread_self()} running add() with [a + b]"
  if (b) {
    return 1 + add(a, b - 1);
  } else {
    return a;
  }
}

int main(int argc, char** argv) {
  // Read in the MAX_ADDAND number

  List Of Thread Id's
  for (int i = 1; i < MAX_ADDAND; i++) {
    for (int j = 1; j <= MAX_ADDAND; j++) {
      // Print "Main starting thread add() for [{i} + {j}]"
      // Create thread
      // Push tid onto List Of Thread Id's
    }
  }

  // for each tid in List of Thread Id's
    // pthread_join(that tid);
    // print "In main, collecting thread {tid} computed [{1} + {1}] = {2}"
  return 0;
}