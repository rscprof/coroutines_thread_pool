#ifndef _COROUTINES_DISPATCHER_H
#define _COROUTINES_DISPATCHER_H
#include "dispatcher.h"
#include<coroutine>
#include<functional>
#include<atomic>

template<class F> class Awaitable {
  public:
    using TASK = std::invoke_result_t<F>;
  private:
    std::optional<std::coroutine_handle<>> handle;
    std::optional<TASK> task;
    std::shared_ptr<dispatcher> disp;
    std::atomic_int counter = 2; 
    void set_handle() {
      if (counter==0) {
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
    Awaitable(Awaitable&&) = delete;

    bool await_suspend(std::coroutine_handle<> h) {
      handle = h;
      counter--;
      set_handle();

      return true;
    }

    bool await_ready() const noexcept {
      if (!task.has_value()) return false;
      return task.value().has_result();
    }

    TASK await_resume() {
      return task.value();
    }

    //Это не обязательно делать thread safe
    Awaitable(const F f,std::shared_ptr<dispatcher> disp):disp(disp) {
      disp->runTask([this,f]() {runAndSignal(f);});
    }

};

class awaitable_factory {
  private:
    std::shared_ptr<dispatcher> disp;
  public:
    template<class F> Awaitable<F> create(const F f) const {
      return Awaitable {f, disp};
    }
    awaitable_factory(std::shared_ptr<dispatcher> disp):disp(disp) {
    }
};


template<class T> struct task
{
  public:
    struct promise_type
    {
      private:
        task<T> ret;
      public:
        task<T> get_return_object() { return ret; }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_value(T res) {
          ret.set_result(res);
        }
        void unhandled_exception() {}
    };


  private:
    class task_internal {
      public:
        task_internal(const task_internal&) = delete;
        task_internal(task_internal&&) = delete;

        std::binary_semaphore hasResult{ 0 };

        std::optional<T> result{};

        std::optional<std::coroutine_handle<>> handle{  };

        task_internal() = default;

        std::atomic_bool resumed = false;

        std::atomic_int counter = 2;

    };

    std::shared_ptr<dispatcher> disp {};

    std::shared_ptr<task_internal> internal{ new task<T>::task_internal {} };
  public:

    T get_value() const {
      return internal->result.value();
    }

    bool has_result() const {
      return internal->result.has_value();
    }

    void set_result(const T& t) {
      internal->result = t;
      internal->hasResult.release();
      internal->counter--;
      resume();
    }

    void set_result(T&& t) {
      internal->result = std::move(t);
      internal->hasResult.release();
      internal->counter--;
      resume();
    }

    void set_handle(const std::coroutine_handle<>& handle,std::shared_ptr<dispatcher> disp) {
      internal->handle = handle;
      this->disp = disp;
      internal->counter--;
      resume();
    }

    void set_handle(std::coroutine_handle<>&& handle,std::shared_ptr<dispatcher> disp) {
      internal->handle = std::move(handle);
      this->disp = disp;
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
          disp->runTask([handle = get_handle().value()]() {handle.resume();});
        }
      }
    }
  public:
    void wait() {
      internal->hasResult.acquire();
    }
};
#endif
