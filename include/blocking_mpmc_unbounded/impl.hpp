#ifndef BLOCKING_MPMC_UNBOUNDED_IMPL
#define BLOCKING_MPMC_UNBOUNDED_IMPL

#include "defs.hpp"

namespace tsfqueue::__impl{
    template <typename T> void blocking_mpmc_unbounded<T>::push(T value) {
        // made the new node unique_ptr 
        std::unique_ptr<node> new_node=std::make_unique<node>();
        // the data is made shared_ptr to avoid crashes
        std::shared_ptr<T> data=std::make_shared<T>(std::move(value));

        // lock the tail mutex so that only this thread can modify
        std::lock_guard<std::mutex> tail_lock(tail_mutex);

        tail->data=std::move(data);
        tail->next=std::move(new_node);

        // tail=tail->next will throw errors due to ownership issues of unique_ptr
        // .get() gives the normal pointer to the new node and we can safely move tail to it
        tail=tail->next.get();

        // can also do artificial scope to avoid locking the size_mutex for too long, but here since we return just after this, it is not necessary
        {
            std::lock_guard<std::mutex> size_lock(size_mutex);
            size_++;
        }
        // notify one thread that is waiting on the condition variable that new data is available
        cond.notify_one();

        return;
    }

    template <typename T> template <typename... Args>
    void blocking_mpmc_unbounded<T>::emplace_back(Args &&...args) {
        std::unique_ptr<node> new_node=std::make_unique<node>();
        std::shared_ptr<T> data=std::make_shared<T>(std::forward<Args>(args)...);

        std::lock_guard<std::mutex> tail_lock(tail_mutex);
        tail->data=std::move(data);
        tail->next=std::move(new_node);
        tail=tail->next.get();

        std::lock_guard<std::mutex> size_lock(size_mutex);
        size_++;

        cond.notify_one();
        return;
    }
    
    template <typename T> typename blocking_mpmc_unbounded<T>::node *blocking_mpmc_unbounded<T>::get_tail() {
        // using the tail mutex only to safely get the tail pointer
        std::lock_guard<std::mutex> tail_lock(tail_mutex);

        return tail;
    }
    
    template <typename T>
    std::unique_ptr<typename blocking_mpmc_unbounded<T>::node> blocking_mpmc_unbounded<T>::wait_and_get() {
        // unique lock is needed for the condition variable
        std::unique_lock<std::mutex> head_lock(head_mutex);

        // waiting using condition variable and not using a while loop as mpmc requires efficient use of CPU
        cond.wait(head_lock, [this](){return !empty();});

        std::unique_ptr<node> old_head=std::move(head);
        // unique_ptr is non copyable but movable, hence std::move
        head=std::move(old_head->next);
        
        std::lock_guard<std::mutex> size_lock(size_mutex);
        size_--;

        return old_head;
    }
    
    template <typename T> std::unique_ptr<typename blocking_mpmc_unbounded<T>::node> blocking_mpmc_unbounded<T>::try_get() {
        std::lock_guard<std::mutex> head_lock(head_mutex);
        // checking only once if empty or not
        if(head.get()!=get_tail()){
            std::unique_ptr<node> old_head=std::move(head);
            head=std::move(old_head->next);

            std::lock_guard<std::mutex> size_lock(size_mutex);
            size_--;

            return old_head;
        }

        return nullptr;
    }
    
    template <typename T> void blocking_mpmc_unbounded<T>::wait_and_pop(T &value) {
        std::unique_ptr<node> old_head=wait_and_get();
        // cannot use *(old_head->data) as data is a shared_ptr, we need to dereference the shared_ptr,which gives a unique_pt which is not copyable
        value=std::move(*(old_head->data));
    }
    
    template <typename T> std::shared_ptr<T> blocking_mpmc_unbounded<T>::wait_and_pop() {
        // get the unique_ptr to the popped node
        std::unique_ptr<node> old_head=wait_and_get();

        // node->data is a shared_ptr by definition
        return old_head->data;
    }
    
    template <typename T> bool blocking_mpmc_unbounded<T>::try_pop(T &value) {
        // get the unique_ptr to the popped node
        std::unique_ptr<node> old_head=try_get();

        if(old_head!=nullptr){
            // cannot use *(old_head->data) as data is a shared_ptr, we need to dereference the shared_ptr,which gives a unique_pt which is not copyable
            value=std::move(*(old_head->data));
            return true;
        }
        
        return false;
    }
    
    template <typename T> std::shared_ptr<T> blocking_mpmc_unbounded<T>::try_pop() {
        // get the unique_ptr to the popped node
        std::unique_ptr<node> old_head=try_get();

        if(old_head!=nullptr){
            return old_head->data;
        }

        return nullptr;
    }
    
    template <typename T> bool blocking_mpmc_unbounded<T>::empty() {
        std::lock_guard<std::mutex> size_lock(size_mutex);
        return size_==0;
    }

    template <typename T> size_t blocking_mpmc_unbounded<T>::size() {
        std::lock_guard<std::mutex> size_lock(size_mutex);
        return size_;
    }
}

#endif

// 1. Add static asserts
// 2. Add emplace_back using perfect forwarding and variadic templates (you
// can use this in push then)
// 3. Add size() function
// 4. Any more suggestions ??