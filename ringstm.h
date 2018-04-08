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


#define CFENCE __asm__ volatile("" ::: "memory")
#define MFENCE __asm__ volatile("mfence" ::: "memory")

// DEF METADATA
#define COMPLETED 0
#define WRITING 1
#define COMMITTED 2

#define BITS 1024


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
uint64_t* accountsAll;
#define ACCOUT_NUM 100000

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
    // if ( ring[RV].status != complete || ring[RV].timestamp < RV) - >RV--
    if (ring[RV].status != COMPLETED || ring[RV].timestamp < RV) {
      RV--;
    }
    CFENCE;
  }

  int64_t tx_read(int64_t *address) {
    //Find the addr is in the write-set signature
    if(writefilter.lookup(address))
    {
     //If found, find the addr is in the write-set & return the value buffered in write-set
     return write_set[address];
    }
    //val = *addr
    CFENCE;
    int64_t val = *address;
    CFENCE;

    //Add addr to read-set signature
    readfilter.add(&address);

    // Check
    tx_validate();
    //Return val
    return val;
  }

  void tx_write(int64_t *address, int64_t value) {
     //Add (or update) the addr and value to the write-set & Add the  addr to the write-set signature
     write_set[address] = value;
     writefilter.add(&address);
  }

  void tx_commit() {
    // if Read Only -> return
    if(write_set.size() == 0) { return; }

    CFENCE;
  again:
    uint64_t commit_time = global_clock;
    tx_validate();

    if(!(__sync_bool_compare_and_swap(&global_clock, commit_time, (commit_time+1)))) {
       goto again;
    }
    /*
    – ring[commit_time + 1] = {writing, write-set-sig, commit_time +1}
    – for (i= commit_time downto RV + 1)
      • if (ring[ ].write-sig ∩ write-set signature)
        – while(ring[i].status == writing) wait
    */
    //For each entry in the write-set
    for(auto& entry : )
    {
      //*entry.addr = entry.value //Write back

    }
    while(ring[commit_time].status == WRITING);
    CFENCE;
    ring[commit_time + 1].status = COMPLETE;

  }

  void tx_validate()  {

    //if Global -Clock == RV -> return
    if(global_clock = RV)
      return;

    int end = global_clock;

    //while (ring[end].timestamp < end) wait


    for(int i = RV+1; i < global_clock; i++)
    {
      if(ring[i].writefilter.intersect(ring[i].readfilter))
      {
          tx_abort();
      }
      if (ring[i].status == WRITING)
      {
        end = i – 1;
      }
    }

    RV = end;

  }

  void tx_abort() {
    ////Just jump back to tx_begin to restart the transaction
    TX_EXCEPTION e;
    throw e;
  }

private:
  std::map<int64_t, int64_t> write_set;
  BitFilter<BITS> writefilter;
  BitFilter<BITS> readfilter;
  uint64_t RV;

};

#endif // RING_STM_H__
