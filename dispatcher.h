#ifndef _DISPATHCHER_H
#define _DISPATHCHER_H

#include "thread_safe_queue.h"
#include <thread>
#include <vector>
#include <functional>
#include <semaphore>
#include <climits>
//counting_semaphor - для получения taskов


class dispatcher {
  public:
    class dispatcher_exception {};
  private:
    std::vector<std::thread> threads;
    thread_safe_queue<std::function<bool()>> queue;
    bool joined = false;
    std::counting_semaphore<INT_MAX> semaphore {0};
#ifdef NDEBUG
    void worker() {
      do {
        semaphore.acquire();
        auto value = queue.pop_front();
        auto f = value.value();
        auto res = f();
        if (!res) break;
      } while (1);
    }
#else 
    void worker() {
      do {
        semaphore.acquire();
        auto value = queue.pop_front();
        if (value.has_value()) {
          auto f = value.value();
          auto res = f();
          if (!res) break;
        } else {
          //По идее это невозможно, надо бы выделить debug/release реализацию
          throw dispatcher_exception();
        }
      } while (1);
    }
#endif
    void runTaskWithoutDecorator(std::function<bool()> f) {
      queue.push_back(f);
      semaphore.release();
    }

    static bool emptyTask() {return false;};

    static bool fullTask(const std::function<void()> f) {
      f();
      return true;
    }

  public:
    void runTask(std::function<void()> f) {
      runTaskWithoutDecorator(std::bind(fullTask,f));
    }

    dispatcher(unsigned int count_threads = std::thread::hardware_concurrency())
      :threads(count_threads)

    {
      for (auto& thread : threads) {
        thread = std::thread(&dispatcher::worker,this);
      }
    }

    dispatcher(const dispatcher&) = delete;
    dispatcher(dispatcher&&) = delete;

    void join() {
      if (!joined) {
        for (decltype(threads.size()) i=0;i<threads.size();i++) {
          runTaskWithoutDecorator(emptyTask);
        }
        for (auto& thread : threads) {
          thread.join();
        }
        joined = true;
      }
    }

    ~dispatcher() {
      join();
    }


};

#endif
