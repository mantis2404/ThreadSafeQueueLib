#ifndef LOCKFREE_SPSC_BOUNDED_IMPL_CT
#define LOCKFREE_SPSC_BOUNDED_IMPL_CT

#include "defs.hpp"

// aliasing wont work here
namespace tsfqueue::__impl {
    template <typename T, size_t Capacity>
    bool lockfree_spsc_bounded<T, Capacity>::try_push(T value) {
        size_t curr_tail=tail.load(std::memory_order_relaxed);
        size_t head_cache=head.load(std::memory_order_acquire);
        size_t next_tail=(curr_tail+1)%capacity;
        
        if(head_cache==next_tail)
            return false;
        
        arr[curr_tail]=value;
        tail.store(next_tail, std::memory_order_release);
        return true;
    }
    
    template <typename T, size_t Capacity>
    void lockfree_spsc_bounded<T, Capacity>::wait_and_push(T value) {
        size_t curr_tail=tail.load(std::memory_order_relaxed);
        size_t next_tail=(curr_tail+1)%capacity;
        
        while(head_cache==next_tail){
            head_cache=head.load(std::memory_order_acquire);
        }
        
        arr[curr_tail]=value;
        tail.store(next_tail, std::memory_order_release);
    }
    
    template <typename T, size_t Capacity>
    bool lockfree_spsc_bounded<T, Capacity>::try_pop(T &value) {
        size_t curr_head=head.load(std::memory_order_relaxed);
        size_t tail_cache=tail.load(std::memory_order_acquire);
        
        if(curr_head==tail_cache)
            return false;
            
        size_t next_head=(curr_head+1)%capacity;
        value=arr[curr_head];
        head.store(next_head, std::memory_order_release);
        return true;
    }
    
    template <typename T, size_t Capacity>
    void lockfree_spsc_bounded<T, Capacity>::wait_and_pop(T &value) {
        size_t curr_head=head.load(std::memory_order_relaxed);
        
        while(curr_head==tail_cache){
            tail_cache=tail.load(std::memory_order_acquire);
        }
            
        size_t next_head=(curr_head+1)%capacity;
        value=arr[curr_head];
        head.store(next_head, std::memory_order_release);
    }
    
    template <typename T, size_t Capacity>
    bool lockfree_spsc_bounded<T, Capacity>::peek(T &value) {
        size_t curr_head=head.load(std::memory_order_relaxed);
        size_t tail_cache=tail.load(std::memory_order_acquire);
        
        if(curr_head==tail_cache)
            return false;
            
        value=arr[curr_head];
        return true;
    }
    
    template <typename T, size_t Capacity> bool lockfree_spsc_bounded<T, Capacity>::empty() {
        size_t curr_head=head.load(std::memory_order_relaxed);
        size_t curr_tail=tail.load(std::memory_order_acquire);
        if(curr_head==curr_tail)
            return true;
        return false;
    }

    template <typename T, size_t Capacity> size_t lockfree_spsc_bounded<T, Capacity>::size() {
        size_t curr_head=head.load(std::memory_order_acquire);
        size_t curr_tail=tail.load(std::memory_order_acquire);
        if(curr_tail>=curr_head)
            return curr_tail-curr_head;
        return capacity-(curr_head-curr_tail);
    }
}


#endif