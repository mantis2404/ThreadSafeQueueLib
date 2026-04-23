#ifndef LOCKFREE_MPSC_UNBOUNDED_DEFS
#define LOCKFREE_MPSC_UNBOUNDED_DEFS

#include "utils.hpp"
#include <atomic>
#include <memory>
#include <type_traits>

namespace tsfqueue::__impl {
template <typename T> class lockfree_mpsc_unbounded {
  // Works exactly same as the blocking_mpmc_unbounded queue (see this once)
  // with tail pointer pointing to stub node and your head pointer updates as
  // per the pushes. See the Lockless_Node in utils to understand the working.
  // Note that the next pointers are atomic there. Why ?? [Reason this]
  // Also the head and tail members are cache-aligned. Why ?? [Reason this] (ask
  // me for details)

  // [Copy of blocking_mpmc_unbounded]
  // For the implementation, we start with a stub node and both head and tail
  // are initialized to it. When we push, we make a new stub node, move the data
  // into the current tail and then change the tail to the new stub. We have two
  // methods : wait_and_pop() which waits on the queue and returns element &
  // try_pop() which returns an element if queue is not empty otherwise returns
  // some neutral element OR a false boolean whichever is applicable. Pop works
  // by returning the data stored in head node and replacing head to its next
  // node. We handle the empty queue gracefully as per the pop type.
private:
  using node = tsfqueue::__utils::Lockless_Node<T>;

  // Add the private members :
 alignas(cache_line_size) std::atomic<node*> head;
 alignas(cache_line_size) std::atomic<node*> tail;
 alignas(cache_line_size) std::atomic<size_t> size_{0};
  // Description of priavte members :
  // 1. node* head -> Pointer to the head node
  // 2. node* tail -> Pointer to tail node
  // 3. size_t capacity -> to track the size of the queue
  // 4. Cache align 1-2

public:
  // Public member functions :
  // Add relevant constructors and destructors -> Add these here only
  lockfree_mpsc_unbounded(){
    node* dummy = new node;
    dummy->next.store(nullptr, std::memory_order_relaxed);
    // no heavy memory orderd like _release used since the queue is being made rn hence can directly fetch from the cache
    head.store(dummy, std::memory_order_relaxed);
    tail.store(dummy, std::memory_order_relaxed);
  }
  ~lockfree_mpsc_unbounded(){
    // head is atomic so we load it first
    node* curr=head.load(std::memory_order_relaxed);
    while(curr!=nullptr){
        // curr->next is also atomic so we need to load it also
        node* next=curr->next.load(std::memory_order_relaxed);
        delete curr;
        curr=next;
    }
  }
  // Pushes the value inside the queue, copies the value
  void push(T value);
  //Blocking wait on queue, returns value in the reference passed as parameter
  void wait_and_pop(T &value);
  // Returns true and gives the value in reference passed, false otherwise
  bool try_pop(T &value);
  // Returns whether the queue is empty or not at that instant
  bool empty();
  // Returns the front/top element of queue in ref (false if empty queue)
  bool peek(T &value);
  size_t size();
  template <typename... Args> void emplace_back(Args &&...args);
  // 6. Add static asserts
  // 7. Add emplace_back using perfect forwarding and variadic templates (you
  // can use this in push then)
  // 8. Add size() function
  // 9. Any more suggestions ??
  // 10. Why no shared_ptr ?? [Reason this]
};
} // namespace tsfqueue::__impl

#endif