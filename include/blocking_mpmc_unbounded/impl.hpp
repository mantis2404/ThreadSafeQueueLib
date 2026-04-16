#ifndef BLOCKING_MPMC_UNBOUNDED_IMPL
#define BLOCKING_MPMC_UNBOUNDED_IMPL

#include "defs.hpp"

template <typename T>
using queue = tsfqueue::__impl::blocking_mpmc_unbounded<T>;

template <typename T> void queue<T>::push(T value) {}

template <typename T> queue<T>::node *queue<T>::get_tail() {}

template <typename T>
std::unique_ptr<queue<T>::node> queue<T>::wait_and_get() {}

template <typename T> std::unique_ptr<queue<T>::node> queue<T>::try_get() {}

template <typename T> void queue<T>::wait_and_pop(T &value) {}

template <typename T> std::shared_ptr<T> queue<T>::wait_and_pop() {}

template <typename T> bool queue<T>::try_pop(T &value) {}

template <typename T> std::shared_ptr<T> queue<T>::try_pop() {}

template <typename T> bool queue<T>::empty() {}

#endif

// 1. Add static asserts
// 2. Add emplace_back using perfect forwarding and variadic templates (you
// can use this in push then)
// 3. Add size() function
// 4. Any more suggestions ??