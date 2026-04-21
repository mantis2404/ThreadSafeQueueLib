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
    // to prevent contention at the head pointer
    // This mutex is acquired when you are modifying std::unique_ptr<node> head to prevent data race.
    std::mutex head_mutex;
    
    // the head pointer. We are using unique_ptr because this will ensure they are deleted automatically and we need not call delete manually. Also see the Node we use from utils have std::unique_ptr<Node<T>> as the next pointers which forms a chain of automatic delete(s).
    std::unique_ptr<node> head;

    // used whenever tail is accessed. Mutex is locked either manually or is locked by our condition variable
    std::mutex tail_mutex;

    // used whenever size is accessed
    std::mutex size_mutex;
    size_t size_;

    //the pointer to tail. Note we cannot have tail as unique_ptr as that would make two unique_ptr(s) to tail (one through) linked list and one through our decalaration. Thus we make this a normal pointer and this pointer is safely deallocated using the linked list unique_ptr during call to destructor
    node *tail;

    // cond is used to check whether queue is empty or not and do a blocking wait on
    std::condition_variable cond;

    // Private member functions :
    // Helper function to get normal pointer to tail at a particular instant 
    node *get_tail();
    // Helper function to blocking wait on unique_ptr of head after popping 
    std::unique_ptr<node> wait_and_get();
    // : Helper function to try to get unique_ptr of head after popping
    std::unique_ptr<node> try_get();

  public:
    // Public member functions :
    // Add relevant constructors and destructors -> Add these here only
    blocking_mpmc_unbounded(){
      head=std::make_unique<node>();
      size_=0;
      tail=head.get();
    }
    ~blocking_mpmc_unbounded()=default;

    // Pushes the value inside the queue, copies the value
    void push(T value);
    // Constructs value in place and pushes into the queue
    template <typename... Args> void emplace_back(Args &&...args);
    // Blocking wait on queue, returns value in the reference passed as parameter
    void wait_and_pop(T &value);
    // Blocking wait on queue, returns value as a shared ptr allocated inside the call
    std::shared_ptr<T> wait_and_pop(void);
    // Tries to pop a value from the queue, returns true and gives the value in reference passed, false otherwise
    bool try_pop(T &value);
    // Returns a shared ptr with data, returns
    // nullptr if failed
    std::shared_ptr<T> try_pop();
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