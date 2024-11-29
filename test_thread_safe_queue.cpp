#include "thread_safe_queue.h"
#include<thread>
#include<vector>
#include<iostream>
#include <syncstream>
int main() {

    const int count_producers_threads = 4;
    const int count_consumers_threads = 4;
    const long long count_numbers = 10000LL;

    thread_safe_queue<long long> queue;
    

     // producer threads
     std::vector<std::thread> producers;
     for (int i = 0; i < count_producers_threads; ++i)
     {
         producers.emplace_back([&queue, i]()
         {
             for (long long j = 0; j < count_numbers; ++j)
             {
                 queue.push_back(i * count_numbers + j);
             }
         });
     }

    std::atomic<long long> sum = 0;
    std::atomic<long long> sum_count = 0;
   
     // consumers thread
     std::vector<std::thread> consumers;
     for (int i = 0; i < count_consumers_threads; ++i)
     {
        
         consumers.emplace_back([&queue, i, &sum,  &sum_count]()
         {
            long long count = 0;
            long long waiting = 0;
            while (true) {
                auto value = queue.pop_front();
                if (value) {
                    if (value==-1) break; 
                    sum +=*value;
                    count++;
                } else waiting++;
             }
             std::osyncstream(std::cout) << "count in "<<i << " consumer is " << count << std::endl;
             std::osyncstream(std::cout) << "waiting in "<<i << " consumer is " << waiting << std::endl;
             sum_count += count;
         });
     }
     
     // join all threads
     for (auto& producer : producers)
     {
         producer.join();
     }

     for (int i = 0;i< count_consumers_threads;i++) {
        queue.push_back(-1);
     }
    

    for (auto& consumer : consumers)
    {
         consumer.join();
    }

    auto max_num = count_numbers*count_producers_threads-1;

    auto correct_sum = max_num*(max_num+1)/2;

    std::cout << "sum count in consumers is " << sum_count << std::endl;


    if (sum == correct_sum) {
        std::cout << "OK" << std::endl;
    } else {
        std::cout << "Error" << "given " << sum <<" correct value " << correct_sum << std::endl;
    }





     return 0;







}