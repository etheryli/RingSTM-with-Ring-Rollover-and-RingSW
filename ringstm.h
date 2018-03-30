#ifndef RING_STM_H__
#define RING_STM_H__

#include "BitFilter.h"

#include "rand_r_32.h"
#include <fcntl.h>
#include <pthread.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define BITS 1024
#define WRITING 0
#define COMPLETED 1
#define COMMITTED 2

// DEF METADATA

struct RingEntry {
  uint64_t timestamp = 0;
  BitFilter<BITS> wf;
  uint64_t priority = 0;
  int status = COMPLETED;
};

// Refactored into class
struct Transaction {
  std::set<*uint64_t> wset;
  BitFilter<BITS> writefilter;
  BitFilter<BITS> readfilter;
  uint64_t start;
};

extern volatile uint64_t global_clock;
extern RingEntry ring[];
extern volatile int ring_index;
extern volatile int prefix_index;
struct TX_EXCEPTION {};

class RingSW {
public:
  void tx_begin() {}

  int64_t tx_read(int64_t *address) {}

  void tx_write(int64_t *address, int64_t value);

  void tx_commit() {}

  void tx_validate() {}

  void tx_abort() {
    TX_EXCEPTION e;
    throw e;
  }

private:
  std::set<*uint64_t> wset;
  BitFilter<BITS> writefilter;
  BitFilter<BITS> readfilter;
  uint64_t start;
};

#endif // RING_STM_H__