#ifndef LOCKFREE_MPMC_BOUNDED_IMPL
#define LOCKFREE_MPMC_BOUNDED_IMPL

#include "defs.hpp"
#include <thread>

namespace tsfqueue::__impl{
    template <typename T, size_t Capacity> bool lockfree_mpmc_bounded<T, Capacity>::try_push(T value) {
        size_t curr_tail=tail.load(std::memory_order_acquire);
        while(true){
            size_t seq = buffer_arr[curr_tail%capacity].sequence.load(std::memory_order_acquire);
            
            intptr_t diff=(intptr_t)seq-(intptr_t)curr_tail;
            // we utilize 100% capacity of the buffer and check the seq no. for every slot
            // if seq no. < pos -> already filled, queue is full
            // if seq no. == pos -> can be filled
            // if seq no. > pos -> another producer filled it, retry

            if(diff==0){
                // multiple threads can satisfy this condition but only one will succeed in CAS and update the tail and rest will fail and retry
                // since it would be on atomic tail variable, only one thread will succeed in updating the tail
                if(tail.compare_exchange_weak(curr_tail, curr_tail+1, std::memory_order_release, std::memory_order_relaxed)){
                    // it can be the case that another thread might start writing the new data on the updated tail even before the current thread has completed writing data on its tail
                    buffer_arr[curr_tail%capacity].data=std::move(value);
                    buffer_arr[curr_tail%capacity].sequence.store(curr_tail+1,std::memory_order_release);

                    return true;
                }
            }
            if(diff>0){
                // another producer filled it, retry
                curr_tail=tail.load(std::memory_order_acquire);
            }
            if(diff<0){
                // queue is full
               return false;
            }           
        }
        return false;
    }

    template <typename T, size_t Capacity> void lockfree_mpmc_bounded<T, Capacity>::wait_and_push(T value) {
        size_t curr_tail=tail.load(std::memory_order_acquire);
        while(true){
            if(try_push(value))
                break;
            std::this_thread::yield(); // brief yield to reduce CPU burn while spinning
        }
    }
    
    template <typename T, size_t Capacity> void lockfree_mpmc_bounded<T, Capacity>::wait_and_pop(T &value) {

    }
    
    template <typename T, size_t Capacity> bool lockfree_mpmc_bounded<T, Capacity >::try_pop(T &value) {
    
    }
    
    template <typename T, size_t Capacity> bool lockfree_mpmc_bounded<T, Capacity>::empty() {

    }

    template <typename T, size_t Capacity> size_t lockfree_mpmc_bounded<T, Capacity>::size() {

    }
}

#endif

// 1. Add static asserts
// 2. Add emplace_back using perfect forwarding and variadic templates (you
// can use this in push then)
// 3. Add size() function
// 4. Any more suggestions ??