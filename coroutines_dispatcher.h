#ifndef _COROUTINES_DISPATCHER_H
#define _COROUTINES_DISPATCHER_H
#include "dispatcher.h"
#include <coroutine>
#include <functional>
#include <optional>
#include <atomic>
#include <memory>
#include "dispatcher.h"
#include <cassert>

template<class F> class Awaitable {
  public:
    using TASK = std::invoke_result_t<F>;
  private:
    std::optional<std::coroutine_handle<>> handle;
    std::optional<TASK> task;
    std::shared_ptr<dispatcher> disp;
    std::atomic_int counter = 2; 
    void set_handle() {
      if (counter.load(std::memory_order_relaxed)==0) {
        task->set_handle(handle.value(),disp);
      }
    }

    //Это выполняется параллельно с await_suspend, await_ready
    void runAndSignal(const std::function<TASK()> f) {
      task = f();
      counter--;
      set_handle();
    }

  public:
    Awaitable(const Awaitable&) = delete;
    Awaitable& operator=(const Awaitable&) = delete;
    Awaitable(Awaitable&&) = delete;
    Awaitable& operator=(Awaitable&&) = delete;

    bool await_suspend(std::coroutine_handle<> h) {
      handle = h;
      counter--;
      set_handle();

      return true;
    }

    bool await_ready() const noexcept {
      return task.has_value() && task.value().has_result();
    }

    TASK await_resume() {
      return task.value();
    }

    //Это не обязательно делать thread safe
    Awaitable(const F& f,const std::shared_ptr<dispatcher>& disp):disp(disp) {
      assert(disp);
      disp->runTask([this,f]() {runAndSignal(f);});
    }

};

class awaitable_factory {
  private:
    std::shared_ptr<dispatcher> disp;
  public:
    template<class F> Awaitable<F> create(const F& f) const {
      return Awaitable {f, disp};
    }
    explicit awaitable_factory(const std::shared_ptr<dispatcher>& disp):disp(disp) {
    }
    awaitable_factory(const awaitable_factory&) = delete;
    awaitable_factory(awaitable_factory&&) = delete;
    awaitable_factory& operator= (const awaitable_factory&) = delete;
    awaitable_factory& operator= (awaitable_factory&&) = delete;
};


template<class T> struct task
{
  public:
    struct promise_type
    {
      private:
        task<T> ret;
      public:
        task<T> get_return_object() const noexcept { return ret; }
        std::suspend_never initial_suspend() const noexcept { return {}; }
        std::suspend_never final_suspend() const noexcept { return {}; }
        void return_value(T res) {
          ret.set_result(res);
        }
        void unhandled_exception() const noexcept {}
    };


  private:
    class task_internal {
      public:
        task_internal(const task_internal&) = delete;
        task_internal(task_internal&&) = delete;
        task_internal& operator= (const task_internal&) = delete;
        task_internal& operator= (task_internal&&) = delete;


        std::binary_semaphore hasResult{ 0 };

        std::optional<T> result{};

        std::optional<std::coroutine_handle<>> handle{  };

        task_internal() = default;

        std::atomic_bool resumed = false;

        std::atomic_int counter = 2;

        std::shared_ptr<dispatcher> disp {};
    };


    std::shared_ptr<task_internal> internal{ std::make_shared<task_internal>() };
  public:

    T get_value() const {
      return internal->result.value();
    }

    bool has_result() const {
      return internal->result.has_value();
    }

    template<class U> void set_result(U&& t) {
      internal->result = std::forward<U>(t);
      internal->hasResult.release();
      internal->counter--;
      resume();
    }

    template<class    U>
    void set_handle(U&& handle,std::shared_ptr<dispatcher> disp) {
      assert(disp);
      internal->handle = std::forward<U>(handle);
      this->internal->disp = disp;
      internal->counter--;
      resume();
    }

    std::optional<std::coroutine_handle<>> get_handle() const {
      return internal->handle;
    }
  private:
    void resume() {
      if (internal->counter==0) {
        bool expected = false;
        if (internal->resumed.compare_exchange_strong(expected,true)) {
          internal->disp->runTask([handle = get_handle().value()]() {handle.resume();});
        }
      }
    }
  public:
    void wait() {
      internal->hasResult.acquire();
    }
};
#endif
