#ifndef UTILS
#define UTILS

#include <memory>
#include <atomic>
#include <new>

namespace tsfqueue::__utils {
template <typename T> struct Node {
  std::shared_ptr<T> data;
  std::unique_ptr<Node<T>> next;
};
template <typename T> struct Lockless_Node {
  T data;
  std::atomic<Lockless_Node *> next;
};
template <typename T> struct buffer_node {
  T data;
  std::atomic<size_t> sequence;
};
} // namespace tsfqueue::__utils

namespace tsfqueue::__impl {
static constexpr size_t cache_line_size =
    std::hardware_destructive_interference_size;
}

#endif