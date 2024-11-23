#ifndef _DISPATHCHER_H
#define _DISPATHCHER_H

#include "thread_safe_queue.h"
#include <thread>
#include <vector>
#include <functional>
#include <semaphore>
#include <limits>
#include <stdexcept>
//counting_semaphor - для получения taskов

class dispatcher {
  public:
    class dispatcher_exception : public std::runtime_error {
      public:
        explicit dispatcher_exception(const char * message):std::runtime_error(message) {}

    };
  private:
    std::vector<std::thread> threads;
    thread_safe_queue<std::function<bool()>> queue;
    bool joined = false;
    std::counting_semaphore<std::numeric_limits<int>::max()> semaphore {0};
    void worker() {
      do {
        semaphore.acquire();
        auto value = queue.pop_front();
        #ifdef NDEBUG
        if (!value.has_value()) {
          throw dispatcher_exception("Unexpected state: queue is empty");
        }
        #endif  
        auto f = value.value();
        auto res = f();
        if (!res) break;
      } while (1);
    }
    void runTaskWithoutDecorator(const std::function<bool()>& f) {
      queue.push_back(f);
      semaphore.release();
    }

    static bool emptyTask() {return false;};

    static bool fullTask(const std::function<void()>& f) {
      f();
      return true;
    }

  public:
    void runTask(const std::function<void()>& f) {
      runTaskWithoutDecorator(std::bind(fullTask,f));
    }

    explicit dispatcher(unsigned int count_threads = std::thread::hardware_concurrency())
      :threads(count_threads)

    {
      for (auto& thread : threads) {
        thread = std::thread(&dispatcher::worker,this);
      }
    }
    dispatcher(const dispatcher&) = delete;
    dispatcher& operator=(const dispatcher&) = delete;
    dispatcher(dispatcher&&) = delete;
    dispatcher& operator=(dispatcher&&) = delete;

    //No thread safe
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
