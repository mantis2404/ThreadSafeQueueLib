#ifndef LOCKFREE_SPSC_BOUNDED_DEFS
#define LOCKFREE_SPSC_BOUNDED_DEFS

#include "utils.hpp"
#include <atomic>
#include <memory>
#include <type_traits>

namespace tsfqueue::__impl {
template <typename T, size_t Capacity> class lockfree_spsc_bounded {
  // For the implementation, we first take the size of the bounded queue from
  // user inside the templates so that we can do compile time memory allocation.
  // We have two atomic pointer, head and tail, tail for pushing the element and
  // head for popping. We also add check tail == head for empty which means one
  // redundant element during allocation. We keep head_cache and tail_cache as
  // cached copies to have a cache efficient code (discuss with me for details).
  // All the data members are cache aligned to prevent cache-line bouncing.
  // The user is provided with both set of functions : try_pop() and try_push()
  // for a wait-free code And wait_and_pop() and wait_and_push() for a lock-less
  // code but not wait-free variant. Thus, the user is given a choice to choose
  // among the preferred endpoints as per use case.
private:
  // consumer modifies head and only reads tail_cache so they can be stored on the same cache line
  // atomic head pointer
  alignas(cache_line_size) std::atomic<size_t> head{0};
  //cached tail pointer
  size_t tail_cache{0};
  // producer modifies tail and only reads head_cache so they can be stored on the same cache line
  // atomic tail pointer
  alignas(cache_line_size) std::atomic<size_t> tail{0};
  // cached head pointer
  size_t head_cache{0};
  // compile time allocated array
  alignas(cache_line_size) T arr[Capacity];
  // we want a compile time constant for capacity to do compile time memory allocation and also to avoid excessive use of memory in case user creates multiple queues of same size, we use static.
  // static variables are not on the same memory lines as the above variables so no need of alignas()
  static constexpr size_t capacity=Capacity;

public:
  static_assert(Capacity > 0, "Capacity must be +ve for ring-buffer SPSC queue");

  // Public Member functions :
  lockfree_spsc_bounded(){};
  ~lockfree_spsc_bounded()=default;
  // Busy wait until element is pushed
  void wait_and_push(T value);
  // Try to push if not full else leave (returns false if could not push, else true)
  bool try_push(T value);
  // Busy wait until we have atmost 1 elt and then pop it and store in reference
  void wait_and_pop(T &value);
  // Try to pop and return false if failed bool
  bool try_pop(T &value);
  // Checks if the queue is empty and return bool
  // used by the consumer only
  bool empty(void);
  // Peek the top of the queue.
  bool peek(T &value);
  // Will work only in SPSC/MPSC why ?? [Reason this]
  // 7. Add static asserts
  // 8. Add emplace_back using perfect forwarding and variadic templates (you can use this in push then)
  size_t size();
  // 10. Any more suggestions ??
  // 11. Why no shared_ptr ?? [Reason this]
};
} // namespace tsfqueue::__impl

#endif