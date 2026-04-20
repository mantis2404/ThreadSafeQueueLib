#ifndef BLOCKING_MPMC_UNBOUNDED_DEFS
#define BLOCKING_MPMC_UNBOUNDED_DEFS

#include "../utils.hpp"
#include <condition_variable>
#include <memory>
#include <mutex>
#include <type_traits>
#include <atomic>   

namespace tsfqueue::__impl
{
  template <typename T>
  class blocking_mpmc_unbounded
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
    using node = tsfqueue::__utils::Node<T>;

    // Add private members :
    std::mutex head_mutex;
    std::unique_ptr<node> head;
    std::mutex tail_mutex;
    node *tail;
    std::condition_variable cond;
    std::atomic<size_t> size_q;

    // Description of private members :
    // 1. std::mutex head_mutex is used to prevent contention at the head pointer
    // This mutex is acquired when you are modifying std::unique_ptr<node> head to
    // prevent data race.

    // 2. std::unique_ptr<node> head is for the head pointer. We are using
    // unique_ptr because this will ensure they are deleted automatically and we
    // need not call delete manually. Also see the Node we use from utils have
    // std::unique_ptr<Node<T>> as the next pointers which forms a chain of
    // automatic delete(s).

    // 3. std::mutex tail_mutex is used whenever tail is accessed. Mutex is locked
    // either manually or is locked by our condition variable

    // 4. node* tail is the pointer to tail. Note we cannot have tail as
    // unique_ptr as that would make two unique_ptr(s) to tail (one through)
    // linked list and one through our decalaration. Thus we make this a normal
    // pointer and this pointer is safely deallocated using the linked list
    // unique_ptr during call to destructor

    // 5. condition_variable cond is used to check whether queue is empty or not
    // and do a blocking wait on

    node *get_tail();
    std::unique_ptr<node> wait_and_get();
    std::unique_ptr<node> try_get();

    // Private member functions :
    // node *get_tail() : Helper function to get normal pointer to tail at a
    // particular instant std::unique_ptr wait_and_get() : Helper function to
    // blocking wait on unique_ptr of head after popping std::unique_ptr try_get()
    // : Helper function to try to get unique_ptr of head after popping

  public:
    // Public member functions :
    blocking_mpmc_unbounded()
    {
      static_assert(!std::is_reference_v<T>,
                    "Queue cannot store reference types.");
      head = std::make_unique<node>();
      tail = head.get();
      size_q.store(0);
    }

    ~blocking_mpmc_unbounded()
    {
      static_assert(
          std::is_destructible_v<T>,
          "Unable to destroy the queue, as the given type is not destructable.");
      while (head)
      {
        head = std::move(head->next);
      }
    }

    // Copy constructor deleted — mutex/condvar/unique_ptr cannot be copied
    blocking_mpmc_unbounded(const blocking_mpmc_unbounded &other) = delete;
    blocking_mpmc_unbounded & operator=(const blocking_mpmc_unbounded &other) = delete;

    // Move constructor deleted — mutex/condvar cannot be moved
    blocking_mpmc_unbounded(blocking_mpmc_unbounded &&other) = delete;
    blocking_mpmc_unbounded &operator=(blocking_mpmc_unbounded &&other) = delete;

    // 1. void push(value) : Pushes the value inside the queue, copies the value
    void push(T);
    // 2. void wait_and_pop(value ref) : Blocking wait on queue, returns value in
    // the reference passed as parameter
    void wait_and_pop(T &);
    // 3. std::shared_ptr wait_and_pop(void) : Blocking wait on queue, returns
    // value as a shared ptr allocated inside the call
    std::shared_ptr<T> wait_and_pop(void);
    // 4. bool try_pop(value ref) : Returns true and gives the value in reference
    // passed, false otherwise
    bool try_pop(T &);
    // 5. std::shared_ptr try_pop() : Returns a shared ptr with data, returns
    // nullptr if failed
    std::shared_ptr<T> try_pop();
    // 6. bool empty() : Returns whether the queue is empty or not at that instant
    bool empty();
    // 7. Add static asserts
    // 8. Add emplace_back using perfect forwarding and variadic templates (you
    // can use this in push then)
    template <typename... Args>
    void emplace_back(Args &&...args);
    // 9. Add size() function
    size_t size();
    // 10. Any more suggestions ??
  };
} // namespace tsfqueue::__impl

#endif