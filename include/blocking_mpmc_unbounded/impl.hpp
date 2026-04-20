#ifndef BLOCKING_MPMC_UNBOUNDED_IMPL
#define BLOCKING_MPMC_UNBOUNDED_IMPL

#include "defs.hpp"

template <typename T>
using queue = tsfqueue::__impl::blocking_mpmc_unbounded<T>;

template <typename T>
void queue<T>::push(T value)
{
    static_assert(
        std::is_copy_constructible_v<T> || std::is_move_constructible_v<T>,
        "T must be copy or move constructible.");
    emplace_back(std::move(value));
}

template <typename T>
queue<T>::node *queue<T>::get_tail()
{
    std::lock_guard<std::mutex> lock(tail_mutex);
    return tail;
}

template <typename T>
std::unique_ptr<typename queue<T>::node> queue<T>::wait_and_get()
{
    std::unique_lock<std::mutex> lock(head_mutex);
    cond.wait(lock, [this]()
              { return head.get() != get_tail(); });
    std::unique_ptr<node>
        pre_head = std::move(head);
    head = std::move(pre_head->next);
    size_q.fetch_sub(1, std::memory_order_relaxed);
    return pre_head;
}

template <typename T>
std::unique_ptr<typename queue<T>::node> queue<T>::try_get()
{
    std::lock_guard<std::mutex> lock(head_mutex);
    if (head.get() == get_tail())
        return nullptr;
    else
    {
        std::unique_ptr<node> pre_head = std::move(head);
        head = std::move(pre_head->next);
        size_q.fetch_sub(1, std::memory_order_relaxed);
        return pre_head;
    }
}

template <typename T>
void queue<T>::wait_and_pop(T &value)
{
    static_assert(
        std::is_copy_constructible_v<T> || std::is_move_constructible_v<T>,
        "T must be copy or move constructible.");
    std::unique_ptr<node> pop_node = wait_and_get();
    value = *pop_node->data;
}

template <typename T>
std::shared_ptr<T> queue<T>::wait_and_pop()
{
    std::unique_ptr<node> pop_node = wait_and_get();
    return pop_node->data;
}

template <typename T>
bool queue<T>::try_pop(T &value)
{
    static_assert(
        std::is_copy_constructible_v<T> || std::is_move_constructible_v<T>,
        "T must be copy or move constructible.");
    std::unique_ptr<node> pop_node = try_get();
    if (pop_node == nullptr)
    {
        return false;
    }
    else
    {
        value = *pop_node->data;
        return true;
    }
}

template <typename T>
std::shared_ptr<T> queue<T>::try_pop()
{
    std::unique_ptr<node> pop_node = try_get();
    if (pop_node == nullptr)
    {
        return nullptr;
    }
    else
    {
        return pop_node->data;
    }
}

template <typename T>
bool queue<T>::empty()
{
    std::lock_guard<std::mutex> l1(head_mutex);
    std::lock_guard<std::mutex> l2(tail_mutex);
    return head.get() == tail;
}

template <typename T>
size_t queue<T>::size()
{
    return size_q.load(std::memory_order_relaxed);
}

template <typename T>
template <typename... Args>
void queue<T>::emplace_back(Args &&...args)
{
    std::unique_ptr<node> new_tail = std::make_unique<node>();
    std::shared_ptr<T> data = std::make_shared<T>(std::forward<Args>(args)...);
    {
        std::lock_guard<std::mutex> lock(tail_mutex);
        tail->data = std::move(data);
        tail->next = std::move(new_tail);
        tail = tail->next.get();
        size_q.fetch_add(1, std::memory_order_relaxed);
    }
    cond.notify_one();
}

#endif

// 1. Add static asserts
// 2. Add emplace_back using perfect forwarding and variadic templates (you
// can use this in push then)
// 3. Add size() function
// 4. Any more suggestions ??