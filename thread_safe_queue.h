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
    std::atomic<item *> next;
    holder_type value;
  };

  std::atomic<item *> first;
  std::atomic<item *> last;

  public:
  static item * make_stub() {
    auto initial_item = new item;
    initial_item->next.store(0,std::memory_order_relaxed);
    initial_item->value = holder_type();
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
    while (pop_front()) ;
    delete first.load(std::memory_order_relaxed);
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
      old_first = first.load(std::memory_order_acquire);
      old_next = old_first->next.load(std::memory_order_relaxed);
      if (!old_next) {
        return std::nullopt; // Queue is empty
      }
    } while (!first.compare_exchange_weak(old_first, old_next, std::memory_order_acq_rel, std::memory_order_acquire));

    auto res = old_first->value;
    delete old_first;
    return res;
  }
};

#endif

