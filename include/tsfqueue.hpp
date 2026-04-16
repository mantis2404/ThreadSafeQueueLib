#ifndef TSFQ_HPP
#define TSFQ_HPP

#include <blocking_mpmc_unbounded/queue.hpp>
#include <lockfree_mpmc_bounded/queue.hpp>
#include <lockfree_mpsc_unbounded/queue.hpp>
#include <lockfree_spsc_bounded/queue.hpp>
#include <lockfree_spsc_unbounded/queue.hpp>

// #define FAST
#ifdef FAST
#include <lockfree_spsc_unbounded_fast/queue.hpp>
#endif

#endif
