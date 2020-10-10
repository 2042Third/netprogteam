// Notes
// No bookcode is available. Simple pthread should be used.

// Use threads similar to fib_thread.c, to add every combination of numbers [1...(MAX_ADDAND-1)] to every combination of numbers [1...MAX_ADDAND]
// using add(),
// then output results after creating all computation threads.
// Need to pthread_join and print the results in the order the threads were made

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>

/* Structure to pass into the threads */
typedef struct AddParams {
  int a;
  int b;
  int sum;
  int print; // Whether to print that it's running or not
} AddParams;

void * threadAdd (void * params_) {
  AddParams* params = (AddParams*) params_;

  // Print "Thread {pthread_self()} running add() with [a + b]"
  if (params->print == 1) {
    printf("Thread %ld running add() with [%d + %d]\n", pthread_self(), params->a, params->b);
  }
  if (params->b) {
    AddParams * input = malloc(sizeof(AddParams));
    input->a = params->a;
    input->b = params->b - 1;
    input->print = 0;

    AddParams * result = (AddParams*) threadAdd((void*) input);
    // Preserve the input's a and b.
    input->b = params->b;
    input->sum = result->sum + 1;
    free(result);

    if (params->print == 1) {
      free(params);
    }
    return (void*) input;
  } else { // Returns {a} as the base case
    AddParams * input = malloc(sizeof(AddParams));
    input->sum = params->a;
    return (void*)input;
  }
}

int main(int argc, char** argv) {
  // Read in the MAX_ADDAND number

  if (argc != 2) {
    fprintf (stderr, "Invalid number of arguments.\n");
    exit(0);
  }

  int MAX_ADDAND = atoi( *(argv+1) );
  int numThreads = MAX_ADDAND * (MAX_ADDAND - 1);

  // List Of Thread Id's
  pthread_t threads[numThreads];
  int num_threads_create = 0;
  for (int i = 1; i < MAX_ADDAND; i++) {
    for (int j = 1; j <= MAX_ADDAND; j++) {
      // Print "Main starting thread add() for [{i} + {j}]"
      printf("Main starting thread add() for [%d + %d]\n", i, j);

      // Create thread
      pthread_t tid;
      AddParams *params = malloc(sizeof(AddParams));
      params->a = i;
      params->b = j;
      params->sum = 0;
      params->print = 1;
      int check_create = pthread_create(&tid, NULL, threadAdd, (void*) params);
      //Check to see if threat creation failed
      if(check_create > 0)
      {
        printf("Thread Creation Failed\n");
      }
      else
      {
        // Push tid onto List Of Thread Id's
        threads[num_threads_create++] = tid;
      }
    }
  }

  AddParams * retval;

  for (int i = 0; i < num_threads_create; i++) {
    int check_join = pthread_join(threads[i], (void**) &retval);
    if(check_join > 0)
    {
      printf("Failed to join Thread %ld\n", threads[i]);
    } else {
      printf("In main, collecting thread %ld computed [%d + %d] = %d\n", threads[i], retval->a, retval->b, retval->sum);
      free(retval);
    }
  }
  return 0;
}