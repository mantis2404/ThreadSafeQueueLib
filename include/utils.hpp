#include <memory>
#include <new>

namespace tsfqueue::__utils {
template <typename T> struct Node {
  std::shared_ptr<T> data;
  std::unique_ptr<Node<T>> next;
};
template <typename T> struct Lockless_Node {
  T data;
  std::atomic<Lockless_Node *> next;
}
} // namespace tsfqueue::__utils

namespace tsfq::__impl {
static constexpr size_t cache_line_size =
    std::hardware_destructive_interference_size;
}