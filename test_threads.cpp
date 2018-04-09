#include "rand_r_32.h"
#include "ringstm.h"
#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#define MAX_ARRAY 1000000

// volatile unsigned int global_clock = 0;
std::vector<int64_t *> accounts;
volatile int total_threads;
volatile int total_accounts;
volatile bool disjointed = false;

unsigned long long throughputs[300];

// Function to measure performance
inline unsigned long long get_real_time() {
  struct timespec time;
  clock_gettime(CLOCK_MONOTONIC_RAW, &time);

  return time.tv_sec * 1000000000L + time.tv_nsec;
}

inline unsigned long long get_real_sec_time() {
  struct timespec time;
  clock_gettime(CLOCK_MONOTONIC_RAW, &time);

  return time.tv_sec;
} // Returns something like 1231 seconds

//  Support a few lightweight barriers
void barrier(int which) {
  static volatile int barriers[16] = {0};
  CFENCE;
  __sync_fetch_and_add(&barriers[which], 1);
  while (barriers[which] != total_threads) {
  }
  CFENCE;
}

void signal_callback_handler(int signum) {
  // Terminate program
  exit(signum);
}

volatile bool ExperimentInProgress = true;
static void catch_SIGALRM(int sig_num) { ExperimentInProgress = false; }

/*********************
 **** th_run *********
 *********************/

void *th_run(void *args) {
  long id = ((long)args);

  barrier(0);

  // Divide the 100,000 transfers equally
  int accounts_per_threads = total_accounts / total_threads;
  int disjoint_min = id * accounts_per_threads;

  if (!disjointed) {
    disjoint_min = 0;
    accounts_per_threads = total_accounts;
  }

  // THROUGHPUT STUFF
  unsigned int seed = id;

  if (id == 0) {
    signal(SIGALRM, catch_SIGALRM);
    alarm(1);
  }

  unsigned long long time = get_real_time();
  int tx_count = 0;

  // Do 100,000/threads# transfers of 10 transfers each
  // Make local STM
  RingSW *s = new RingSW();
  bool again = true;
  while (ExperimentInProgress) {
    int acc1[1000];
    int acc2[1000];

    // GENERATE RAND NUMBER OF ACCOUNTS
    again = true;
    do {
      try {
        for (int i = 0; i < 10; i++) {
          acc1[i] = (rand_r_32(&seed) % accounts_per_threads) + disjoint_min;
          acc2[i] = (rand_r_32(&seed) % accounts_per_threads) + disjoint_min;
        }

        tx_count++;

        s->tx_begin();
        for (int i = 0; i < 10; i++) {
          if (s->tx_read(accounts[acc2[i]]) >= 50) {
            s->tx_write(accounts[acc2[i]], s->tx_read(accounts[acc2[i]]) - 50);
            s->tx_write(accounts[acc1[i]], s->tx_read(accounts[acc1[i]]) + 50);
          }
        }
        s->tx_commit();
        again = false;
        s->commits++;
      } catch (TX_EXCEPTION e) {
        again = true;
        s->aborts++;
      }
    } while (again);
  }
  time = get_real_time() - time;
  throughputs[id] = (1000000000LL * tx_count) / (time);

  printf("This id is %ld: commits = %ld, aborts = %ld\n", id, s->commits,
         s->aborts);
  delete s;
  return 0;
}

/*******************
 **** MAIN *********
 *******************/

int main(int argc, char *argv[]) {

  signal(SIGINT, signal_callback_handler);

  if (argc < 3 || argc > 4) {
    printf("Usage test <threads #> <accounts #> <-d>\n");
    exit(0);
  }

  total_threads = atoi(argv[1]);

  // Additional commandline argument for number of accounts and disjoint flag
  total_accounts = atoi(argv[2]);
  if (total_accounts > MAX_ARRAY || total_accounts <= 0) {
    printf("total accounts out of range\n");
    exit(0);
  }

  accounts.resize(total_accounts);
  for (int i = 0; i < total_accounts; i++) {
    accounts[i] = new int64_t(1000);
  }

  if (argc == 4)
    disjointed = (std::string(argv[3]) == "-d");

  pthread_attr_t thread_attr;
  pthread_attr_init(&thread_attr);

  pthread_t client_th[300];
  long ids = 1;
  for (int i = 1; i < total_threads; i++) {
    pthread_create(&client_th[ids - 1], &thread_attr, th_run, (void *)ids);
    ids++;
  }

  printf("Threads: %d created\n", total_threads);

  int start_sum = 0;
  for (int i = 0; i < total_accounts; i++) {
    start_sum += *accounts[i];
  }

  printf("Start total balance for %d accounts: $%d\n", total_accounts,
         start_sum);

  unsigned long long start = get_real_time();

  th_run(0);

  for (int i = 0; i < ids - 1; i++) {
    pthread_join(client_th[i], NULL);
  }

  printf("Total time = %lld ns\n", get_real_time() - start);

  // DEBUG for accounts changed
  int final_sum = 0;
  for (int i = 0; i < total_accounts; i++) {
    final_sum += *accounts[i];
  }

  // Throughputs total
  unsigned long long totalThroughput = 0;
  for (int i = 0; i < total_threads; i++) {
    totalThroughput += throughputs[i];
  }

  printf("\nThroughput = %llu\n", totalThroughput);

  printf("Final total balance for %d accounts: $%d\n", total_accounts,
         final_sum);
  printf("Clock: %lu\n", global_clock);

  for (int i = 0; i < total_accounts; i++) {
    delete accounts[i];
  }

  return 0;
}

// Build with
// g++ FILE -o NAME -lpthread -std=c++11

// run with: ( -d = disjoint flag)
// NAME <threads #> <accounts #> <-d>
