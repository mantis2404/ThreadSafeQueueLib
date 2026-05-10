#ifndef LOCKFREE_MPMC_BOUNDED_DEFS
#define LOCKFREE_MPMC_BOUNDED_DEFS

#include "../utils.hpp"
#include <vector>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <type_traits>
#include <atomic>   

namespace tsfqueue::__impl
{
  template <typename T,size_t Capacity>
  class lockfree_mpmc_bounded
  {
    // For the implementation, we start with a stub node and both head and tail
    // are initialized to it. When we push, we make a new stub node, move the data
    // into the current tail and then change the tail to the new stub. We have two
    // methods : wait_and_pop() which waits on the queue and returns element &
    // try_pop() which returns an element if queue is not empty otherwise returns
    // some neutral element OR a false boolean whichever is applicable. Pop works
    // by returning the data stored in head node and replacing head to its next
    // node. We handle the empty queue gracefully as per the pop type.
  private:
    using buffer = tsfqueue::__utils::buffer_node<T>;
    // Add private members :
    alignas(cache_line_size) std::atomic<size_t> head{0};
    //cached tail pointer
    size_t tail_cache{0};
    // producer modifies tail and only reads head_cache so they can be stored on the same cache line
    // atomic tail pointer
    alignas(cache_line_size) std::atomic<size_t> tail{0};
    // creating a ring buffer through custom vector of buffer storing the data and sequence number
    alignas(cache_line_size) std::unique_ptr<buffer[]> buffer_arr;
    // we want a compile time constant for capacity to do compile time memory allocation and also to avoid excessive use of memory in case user creates multiple queues of same size, we use static.
    // static variables are not on the same memory lines as the above variables so no need of alignas()
    static constexpr size_t capacity=Capacity;

  public:
    // Public member functions :
    // Add relevant constructors and destructors -> Add these here only
    // used array as vector requrie copyable/movable type and we used atomic variable in our struct
    lockfree_mpmc_bounded() : buffer_arr(new buffer[capacity]) {
        for (size_t i=0; i<capacity;i++) {
            buffer_arr[i].data = T();
            buffer_arr[i].sequence.store(i,std::memory_order_relaxed);
        }
    }
    ~lockfree_mpmc_bounded()=default;
    
    // tries to push the value inside the queue if not full
    bool try_push(T value);
    // Pushes the value inside the queue, copies the value
    void wait_and_push(T value);
    // Constructs value in place and pushes into the queue
    template <typename... Args> void emplace_back(Args &&...args);
    // Blocking wait on queue, returns value in the reference passed as parameter
    void wait_and_pop(T &value);
    // Tries to pop a value from the queue, returns true and gives the value in reference passed, false otherwise
    bool try_pop(T &value);
    // Returns whether the queue is empty or not at that instant
    bool empty();
    size_t size();
    // 7. Add static asserts
    // 8. Add emplace_back using perfect forwarding and variadic templates (you
    // can use this in push then)
    // 9. Add size() function
    // 10. Any more suggestions ??
  };
} // namespace tsfqueue::__impl

#endif