#ifndef BUFFER_CACHE_GLOBAL_PAGE_REPL_HPP_
#define BUFFER_CACHE_GLOBAL_PAGE_REPL_HPP_

#include <stdint.h>

#include "concurrency/one_per_thread.hpp"
#include "errors.hpp"

// global_page_repl_t is responsible for fair page replacement across the entire
// process, across different threads!
//
// Suppose we have caches x_1, ..., x_N, where weight(x_i) is the number of bytes held
// in memory by cache x_i.
//
// When we need to evict an amount of memory, we randomly choose a cache with
// probability weighted by weight(x_i) and tell that cache to evict that amount of
// memory. [1]
//
// Caches' weights grow when they load a block that's on disk -- they don't self-evict.
//
// Because the system is multithreaded, global_page_repl_t has to operate with
// time-delayed information.  Some time might pass between a block being allocated and
// a subsequent block being evicted.  If the message hub gets backed up, there could be
// large amounts of time delay.  We can't allocate an unbounded amount of memory,
// promising to evict it later, because we'll overflow the memory limit and start
// thrashing.
//
// The system obeys [2] the hard memory limit except under extreme duress.  [3]
//
// To obey the hard memory limit, each thread operates independently (in a "thread page
// repl") with a hard limit.  Obviously they try to use as much of their limit as
// possible.  Thread page repls keep track of how much eviction they're doing, and
// memory allowances are redistributed between threads to keep the current eviction
// rates proportional to each thread page repl's allowance.
//
//
// [1] For the sake of fairness, it is important that each cache try to evict specific
// amounts of memory, not nebulous units such as "one block".  If a cache can't evict
// that exact amount of memory (because of variable block sizes), it should evict more
// than that amount, and credit itself the difference.  Its amount evicted would always
// be in [X, X + maximum_block_size), where X is the total amount of evictions the
// global_page_repl_t has demanded of it.  Of course, when under extreme duress, a
// cache could refuse to evict anything.
//
// [2] "Obeys" might be a bit loose here.  We're talking about memory that's been
// accounted for, not all the unaccounted memory usage that goes on in metadata
// structures, allocation overhead, temporarily loaded readahead blocks, etc.
//
// [3] "Duress" can occur if an mc_transaction_t acquires all the blocks and keeps
// trying to acquire more.  We could drain every other transaction, but not that one.
// Duress can be handled on a per-thread basis -- if one cache claims it can't evict
// _anything_, then another cache will be selected on the same thread until we find one
// that can.  Individual global_page_repl_t threads can operate independently if they
// run out of "reserve".

class thread_page_repl_t;

class global_page_repl_t {
public:
    // RSI: Do something about this default value.
    global_page_repl_t(uint64_t memory_limit = (1 << 28));
    ~global_page_repl_t();

    void change_memory_limit(uint64_t new_memory_limit);

private:
    uint64_t memory_limit_;
    // RSI: one_per_thread_t uses get_num_threads() instead of get_num_db_threads.
    one_per_thread_t<thread_page_repl_t> thread_page_repl_;
    DISABLE_COPYING(global_page_repl_t);
};

#endif  // BUFFER_CACHE_GLOBAL_PAGE_REPL_HPP_
