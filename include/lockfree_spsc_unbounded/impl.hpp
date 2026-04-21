#ifndef LOCKFREE_SPSC_UNBOUNDED_IMPL
#define LOCKFREE_SPSC_UNBOUNDED_IMPL

#include "defs.hpp"

// since the nodes are atomic we need to use .load(memory_order_relaxed) to first fetch it an then perform operations on it
// to assign a value we will have to do .store()
// not to use = with atomics
namespace tsfqueue::__impl {
    template <typename T>
    void lockfree_spsc_unbounded<T>::push(T value) {
        emplace_back(std::move(value));
        // no extra copies of value made, the pointer is directly moved to the new node,
        // the value is destroyed at the end of the scope.
    }
    
    template <typename T>
    bool lockfree_spsc_unbounded<T>::try_pop(T &value) {
        node* curr_head=head.load(std::memory_order_relaxed);
        node* next_head=curr_head->next.load(std::memory_order_acquire);
        
        if(next_head==nullptr)
            return false;
            
        value=next_head->data;
        delete curr_head;
        head.store(next_head, std::memory_order_relaxed);
        capacity.fetch_sub(1, std::memory_order_relaxed);
        return true;
    }
    
    template <typename T>
    void lockfree_spsc_unbounded<T>::wait_and_pop(T &value) {
        node* curr_head=head.load(std::memory_order_relaxed);
        node* new_head=nullptr;

        while(new_head==nullptr){
            new_head=head.load(std::memory_order_relaxed)->next.load(std::memory_order_acquire);
        }
            
        delete curr_head;
        head.store(new_head, std::memory_order_relaxed);
        capacity.fetch_sub(1, std::memory_order_relaxed);
    }
    
    template <typename T>
    bool lockfree_spsc_unbounded<T>::peek(T &value) {
        if(head.load(std::memory_order_relaxed)->next.load(std::memory_order_acquire)==nullptr)
            return false;
            
        value=head.load(std::memory_order_relaxed)->data;
        return true;
    }
    
    template <typename T> bool lockfree_spsc_unbounded<T>::empty() {
        if(head.load(std::memory_order_relaxed)->next.load(std::memory_order_relaxed)==nullptr)
            return true;
        
        return false;
    }

    template <typename T> size_t lockfree_spsc_unbounded<T>::size() {
        return capacity.load(std::memory_order_relaxed);
    }

    template <typename T> template <typename... Args>
    void lockfree_spsc_unbounded<T>::emplace_back(Args&&... args) {
    
        node* new_node=new node();
        new_node->data=T(std::forward<Args>(args)...); 
        new_node->next.store(nullptr, std::memory_order_relaxed);

        node* prev_tail=tail.load(std::memory_order_relaxed);
        prev_tail->next.store(new_node, std::memory_order_release); 

        capacity.fetch_add(1, std::memory_order_relaxed);
        tail.store(new_node, std::memory_order_relaxed);
    }
}

#endif

// 1. Add static asserts
// 2. Add emplace_back using perfect forwarding and variadic templates (you
// can use this in push then)
// 3. Add size() function
// 4. Any more suggestions ??
