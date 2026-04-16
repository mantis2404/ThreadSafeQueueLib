#ifndef LOCKFREE_SPSC_UNBOUNDED_IMPL
#define LOCKFREE_SPSC_UNBOUNDED_IMPL

#include "defs.hpp"

namespace tsfqueue::__impl {
    template <typename T>
    void lockfree_spsc_unbounded<T>::push(T value) {
    }
    
    template <typename T>
    bool lockfree_spsc_unbounded<T>::try_pop(T &value) {
        node* curr_head=head;
        node* next_head=head->next.load(std::memory_order_acquire);
        
        if(next_head==nullptr)
            return false;
            
        delete curr_head;
        head=next_head;
        capacity.fetch_sub(1, std::memory_order_relaxed);
        return true;
    }
    
    template <typename T>
    void lockfree_spsc_unbounded<T>::wait_and_pop(T &value) {
        node* curr_head=head;
        node* new_head=nullptr;

        while(new_head==nullptr){
            new_head=head->next.load(std::memory_order_acquire);
        }
            
        delete curr_head;
        head=new_head;
        capacity.fetch_sub(1, std::memory_order_relaxed);
    }
    
    template <typename T>
    bool lockfree_spsc_unbounded<T>::peek(T &value) {
        if(head->next.load(std::memory_order_acquire)==nullptr)
            return false;
            
        value=head->data;
        return true;
    }
    
    template <typename T> bool lockfree_spsc_unbounded<T>::empty() {
        if(head->next.load(std::memory_order_acquire)==nullptr)
            return false;
        
        return true;
    }

    template <typename T> size_t lockfree_spsc_unbounded<T>::size() {
        return capacity.load(std::memory_order_relaxed);
    }
}

#endif

// 1. Add static asserts
// 2. Add emplace_back using perfect forwarding and variadic templates (you
// can use this in push then)
// 3. Add size() function
// 4. Any more suggestions ??