#ifndef LOCKFREE_SPSC_UNBOUNDED_IMPL
#define LOCKFREE_SPSC_UNBOUNDED_IMPL

#include "defs.hpp"

template <typename T>
using queue = tsfqueue::__impl::lockfree_spsc_unbounded<T>;

template <typename T> void queue<T>::push(T value) {}

template <typename T> bool queue<T>::try_pop(T &value) {}

template <typename T> void queue<T>::wait_and_pop(T &value) {}

template <typename T> bool queue<T>::peek(T &value) {}

template <typename T> bool queue<T>::empty(void) {}

#endif

// 1. Add static asserts
// 2. Add emplace_back using perfect forwarding and variadic templates (you
// can use this in push then)
// 3. Add size() function
// 4. Any more suggestions ??