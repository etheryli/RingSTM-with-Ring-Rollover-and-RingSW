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
  void tx_begin()
  {
    /*
    - Reset thread-local metadata
    - RV = Global-Clock
    - while ( ring[RV].status != complete || ring[RV].timestamp < RV)
    - TX.start--
    - fence(Read-Before-Read)
    */
  }

  int64_t tx_read(int64_t *address)
  {
    /*
  - Find the addr is in the write-set signature
    • If found, find the addr is in the write-set
      – return the value buffered in write-set
  – val = *addr
  – Add addr to read-set signature
  – tx_validate()
  – Return val
  */
  }

  void tx_write(int64_t *address, int64_t value)
  {
    /*
   - Add (or update) the addr and value to the write-set
   - Add the  addr to the write-set signature
   */
  }

  void tx_commit()
  {
    /*
    – if (write-set.size == 0) -> return  //read-only tx
    – again : commit_time = Global-Clock
    – tx_validate()
    – If (!CAS(&Global-Clock, commit_time, commit_time+1))
      • goto again
    – ring[commit_time + 1] = {writing, write-set-sig, commit_time +1}
    – for (i= commit_time downto RV + 1)
      • if (ring[ ].write-sig ∩ write-set signature)
        – while(ring[i].status == writing) wait
    – For each entry in the write-set
      • *entry.addr = entry.value //Write back
    – while(ring[commit_time.status == writing) wait
    – ring[commit_time + 1].status = complete
    */
  }

  void tx_validate()
  {
    /*
  - if Global -Clock == RV -> return
  – end = Global-Clock
  – while (ring[end].timestamp < end) wait
  – for ring entries between Global-Clock & RV+1
    • if (ring-entry.write-sig ∩ read-set signature)
      – tx_abort ()
    • if (ring-entry.status  == writing)
      – end = (ring-entry-index) – 1
  – RV = end
  */
  }

  void tx_abort() {
    ////Just jump back to tx_begin to restart the transaction
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
