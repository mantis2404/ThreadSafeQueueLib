#ifndef LOCKFREE_MPSC_UNBOUNDED_IMPL
#define LOCKFREE_MPSC_UNBOUNDED_IMPL

#include "defs.hpp"

// since the nodes are atomic we need to use .load(memory_order_relaxed) to first fetch it an then perform operations on it
// to assign a value we will have to do .store()
// not to use = with atomics
namespace tsfqueue::__impl {
    template <typename T>
    void lockfree_mpsc_unbounded<T>::push(T value) {
        node* new_node=new node();
        new_node->data=std::move(value);
        new_node->next.store(nullptr, std::memory_order_relaxed);

        // Atomically publish the new tail and get the previous tail.
        node* old_tail=tail.exchange(new_node,std::memory_order_acq_rel);

        old_tail->next.store(new_node,std::memory_order_release);

        size_.fetch_add(1, std::memory_order_relaxed);
    }
    
    template <typename T>
    bool lockfree_mpsc_unbounded<T>::try_pop(T &value) {
        node* curr_head=head.load(std::memory_order_relaxed);
        node* next_head=curr_head->next.load(std::memory_order_acquire);
        
        if(next_head==nullptr)
            return false;
            
        value=next_head->data;
        delete curr_head;
        head.store(next_head, std::memory_order_relaxed);
        size_.fetch_sub(1, std::memory_order_relaxed);
        return true;
    }
    
    template <typename T> void lockfree_mpsc_unbounded<T>::wait_and_pop(T &value) {
        node* curr_head=head.load(std::memory_order_relaxed);
        node* next_head=nullptr;

        while(next_head==nullptr){
            next_head=curr_head->next.load(std::memory_order_acquire);
        }
        
        // as per the dummy node approach we pop the head->next eleme becasue the head points to a nullptr data node at the start
        value=next_head->data;
        delete curr_head;
        head.store(next_head, std::memory_order_relaxed);
        size_.fetch_sub(1, std::memory_order_relaxed);
    }
    
    template <typename T> bool lockfree_mpsc_unbounded<T>::peek(T &value) {
        // only one consumer can modify head
        node* curr_head=head.load(std::memory_order_relaxed);
        node* next_head=curr_head->next.load(std::memory_order_acquire);
        
        if(next_head==nullptr)
            return false;
            
        value=next_head->data;
        return true;
    }
    
    template <typename T> bool lockfree_mpsc_unbounded<T>::empty() {
        if(head.load(std::memory_order_relaxed)->next.load(std::memory_order_acquire)==nullptr)
            return true;
        
        return false;
    }

    template <typename T> size_t lockfree_mpsc_unbounded<T>::size() {
        return size_.load(std::memory_order_acquire);
    }
    
    // no extra copies of value made, the pointer is directly moved to the new node,
    // the value is destroyed at the end of the scope.
    template <typename T> template <typename... Args>
    void lockfree_mpsc_unbounded<T>::emplace_back(Args&&... args) {
        node* new_node=new node();
        new_node->data=T(std::forward<Args>(args)...); 
        new_node->next.store(nullptr, std::memory_order_relaxed);

        // Atomically publish the new tail and get the previous tail.
        node* old_tail=tail.exchange(new_node,std::memory_order_acq_rel);

        old_tail->next.store(new_node,std::memory_order_release);

        size_.fetch_add(1, std::memory_order_relaxed);
    }
}

#endif

// 1. Add static asserts
// 2. Add emplace_back using perfect forwarding and variadic templates (you
// can use this in push then)
// 3. Add size() function
// 4. Any more suggestions ??
