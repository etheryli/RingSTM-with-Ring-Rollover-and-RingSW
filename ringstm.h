#ifndef RING_STM_H__
#define RING_STM_H__

#include "BitFilter.h"
#include "rand_r_32.h"
#include <fcntl.h>
#include <map>
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

#define CFENCE __asm__ volatile("" ::: "memory")
#define MFENCE __asm__ volatile("mfence" ::: "memory")

// DEFINE METADATA
#define COMPLETED 0
#define WRITING 1

#define BITS 64

#define RING_SIZE 8

struct RingEntry {
  volatile uint64_t timestamp = 0;
  BitFilter<BITS> wf;
  volatile uint64_t priority = 0;
  volatile int status = COMPLETED;
};

volatile uint64_t global_clock = 0;

// Ring struct that cleans itself after program ends
struct Ring {
  Ring() { array = new RingEntry[RING_SIZE]; }
  ~Ring() { delete[] array; }
  RingEntry &operator[](uint64_t index) { return array[index]; }
  RingEntry *array;
};

Ring ring;

struct TX_EXCEPTION {};

/*
1.
    whenever the expected
    timestamp of an entry is greater than the value expected
    by a transaction (either during validate or commit),
    that transaction must abort.

2.  a rollover test must be issued before returning, to ensure
    that, after all validation is complete, the oldest entry validated
    has not been overwritten (accomplished with a test on
    its timestamp field).This test detects when a validation is
    interrupted by a context switch, and conservatively aborts
    if ring rollover occurs while the transaction is swapped out.

3.
    During initialization, updates to a new ring entry’s timestamp
    must follow the setting of the write filter and the status
    (write-after-write ordering). Calls to check from tm_end
    must block until the newest ring entry’s timestamp is set.
*/

class RingSW {
public:
  void tx_begin() {
    // Reset thread-local metadata
    writefilter.clear();
    readfilter.clear();
    write_set.clear();

    CFENCE;
    // RV = Global-Clock
    RV = global_clock;
    uint64_t index = RV % RING_SIZE;
    // if ( ring[RV].status != complete || ring[RV].timestamp < RV) - >RV--
    if (ring[index].status != COMPLETED || ring[index].timestamp < RV) {
      RV--;
    }
    CFENCE;
  }

  int64_t tx_read(int64_t *address) {
    // Find the addr is in the write-set signature
    if (writefilter.lookup(address)) {
      // If found, find the addr is in the write-set & return the value buffered
      // in write-set (IF FOUND)
      if (write_set.count(address) > 0)
        return write_set[address];
    }
    // val = *addr
    CFENCE;
    int64_t val = *address;
    CFENCE;

    // Add addr to read-set signature
    readfilter.add(address);

    // Check
    tx_validate();
    // Return val
    return val;
  }

  void tx_write(int64_t *address, int64_t value) {
    // Add (or update) the addr and value to the write-set & Add the  addr to
    // the write-set signature
    write_set[address] = value; // This operator updates or inserts
    writefilter.add(address);
  }

  void tx_validate() {
    CFENCE;
    uint64_t end = global_clock;
    CFENCE;

    uint64_t end_index = end % RING_SIZE;

    // greater than expected Check (1)
    /*if (ring[end_index].timestamp > end) {
      tx_abort();
    }*/
    for (int i = 0; i < RING_SIZE; i++) {
      if (ring[i].timestamp > end) {
        tx_abort();
      }
    }

    // if Global -Clock == RV -> return

    if (end == RV)
      return;

    while (ring[end_index].timestamp < end) {
      // WAIT
    }

    for (uint64_t i = end; i > RV; i--) {
      int index = i % RING_SIZE;
      if (ring[index].wf.intersect(&readfilter)) {
        tx_abort();
      }
    }

    while (ring[end_index].status != COMPLETED) {
      // WAIT
    }

    // Rollover test (2)
    uint64_t rollover_check_index = (RV) % RING_SIZE;
    if (ring[rollover_check_index].timestamp != (RV)) {
      tx_abort();
    }

    CFENCE;
    RV = end;
    CFENCE;
  }

  void tx_commit() {
    // if Read Only -> return
    if (write_set.size() == 0) {
      return;
    }

  again:
    CFENCE;
    uint64_t commit_time = global_clock;
    CFENCE;

    // (3) Calls to check from tm_end
    // must block until the newest ring entry’s timestamp is set.
    uint64_t commit_time_index = commit_time % RING_SIZE;
    while (ring[commit_time_index].timestamp != commit_time) {
      // Block
    }

    tx_validate();

    if (!(__sync_bool_compare_and_swap(&global_clock, commit_time,
                                       (commit_time + 1)))) {
      goto again;
    }

    uint64_t new_index = (commit_time + 1) % RING_SIZE;

    CFENCE;
    ring[new_index].status = WRITING;
    ring[new_index].wf.fastcopy(&writefilter);
    ring[new_index].timestamp = commit_time + 1;
    CFENCE;

    // For each entry in the write-set
    CFENCE;
    for (auto &entry : write_set) {
      *entry.first = entry.second; // Write back
    }
    CFENCE;

    CFENCE;
    ring[new_index].status = COMPLETED;
    commits++;
    CFENCE;
  }

  void tx_abort() {
    ////Just jump back to tx_begin to restart the transaction
    aborts++;
    TX_EXCEPTION e;
    throw e;
  }

  long commits = 0;
  long aborts = 0;

private:
  std::map<int64_t *, int64_t> write_set;
  BitFilter<BITS> writefilter;
  BitFilter<BITS> readfilter;
  uint64_t RV;
};

#endif // RING_STM_H__
