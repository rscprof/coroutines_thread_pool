#include"coroutines_dispatcher.h"
#include<vector>
#include<ranges>
#include<iostream>
#include<numeric>

template<class T> concept sized_and_random_access_range =
std::ranges::sized_range<T> && std::ranges::random_access_range<T>;

template<class T> concept addable =
requires (T x, T y) {
  { x + y } -> std::convertible_to<T>;
};



template<class T>
concept sized_random_access_range_with_addable_items =
sized_and_random_access_range<T> &&
addable<std::ranges::range_value_t<T>>;


//С пустыми range корректно не работает
template<sized_random_access_range_with_addable_items RANGE>
task<std::ranges::range_value_t<RANGE>> calc(RANGE v, const awaitable_factory& factory,std::ranges::range_size_t<RANGE> threshold = 10000) {
  auto size = v.size();
  if (size < threshold) {
    auto result =  std::accumulate(v.begin() + 1, v.end(), *v.begin());
    co_return result;
  }
  else {
    auto begin = v.begin();
    auto end = v.end();
    auto middle = begin + size/2;
    auto task1 = factory.create([begin, middle, threshold,&factory]() {
        return calc(std::ranges::subrange(begin, middle), factory,threshold);
        });
    auto task2 = factory.create([middle, end, threshold,&factory]() {
        return calc(std::ranges::subrange(middle, end), factory,threshold);
        });
    auto result1 = co_await task1;
    auto result2 = co_await task2;
    auto result = result1.get_value() + result2.get_value();
    co_return result;
  }

}

int main() {
  auto disp = std::make_shared<dispatcher>(4);
  auto factory = awaitable_factory(disp);
  std::vector<long long> v(1000000);
  std::iota(v.begin(), v.end(), 1);
  auto res = calc(v,factory);
  res.wait();
  auto result = res.get_value();
  std::cout << std::endl << result  << std::endl;
}

