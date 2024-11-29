#ifndef _THREAD_SAFE_QUEUE_H
#define _THREAD_SAFE_QUEUE_H
#include <new> //for bad_alloc
#include <optional>
#include <atomic>
#include <memory>
#include <type_traits>
#include <utility>

template<class T>
class thread_safe_queue {
  using holder_type = std::conditional_t<std::is_default_constructible_v<T>, T, std::optional<T>>;

  struct item {
    std::atomic<item *> next {nullptr};
    holder_type value {};
    
    item() = default;

    template<class U>
    explicit item (U&& val):value(std::forward<U>(val)) {}

  };

  std::atomic<item *> first;
  std::atomic<item *> last;

  public:
  static item * make_stub() {
    auto initial_item = new item;
    return initial_item; 
  }

  //constructor is not thread safe
  thread_safe_queue() {
    auto initial_item = make_stub();
    last.store(initial_item, std::memory_order_relaxed);
    first.store(initial_item, std::memory_order_relaxed);

  }

  //destructor is not thread safe
  ~thread_safe_queue() {
    auto old_first = first.load(std::memory_order_relaxed);
    do {
      auto old_next = old_first->next.load(std::memory_order_relaxed);
      delete old_first;
      old_first = old_next;
    } while(old_first);
  }

  template<class U> void push_back(U&& elem) {

    auto new_item = make_stub();

    item *  old_last;
    do {
      old_last = last.load(std::memory_order_acquire);
    } while (!last.compare_exchange_weak(old_last, new_item, std::memory_order_acq_rel, std::memory_order_acquire));

    old_last->value = std::forward<U>(elem);
    old_last->next.store(new_item,std::memory_order_release);
  }

  std::optional<T> pop_front() {
    item * old_first;
    item * old_next;
    do {
      do {
        old_first = first.load(std::memory_order_acquire);
      } while (!first.compare_exchange_weak(old_first, 0, std::memory_order_acq_rel, std::memory_order_acquire         ));
    } while (!old_first);
    old_next = old_first->next.load(std::memory_order_relaxed);
    if (!old_next) {
        first.store(old_first,std::memory_order_release);
        return std::nullopt; // Queue is empty
    }

    first.store(old_next,std::memory_order_release);
    auto res = std::move(old_first->value);
    delete old_first;
    return res;
  }

  thread_safe_queue(const thread_safe_queue&) = delete;
  thread_safe_queue& operator=(const thread_safe_queue&) = delete;
  thread_safe_queue(thread_safe_queue&&) = delete;
  thread_safe_queue& operator=(thread_safe_queue&&) = delete;
};

#endif

