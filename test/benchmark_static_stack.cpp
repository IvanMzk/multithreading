#include <thread>
#include <iostream>
#include <array>
#include <chrono>
#include "catch.hpp"
#include "static_stack.hpp"
#include "benchmark_helpers.hpp"
#include "spinlock_mutex.hpp"


TEMPLATE_TEST_CASE("benchmark_static_stack","[benchmark_static_stack]",
    std::mutex,
    experimental_multithreading::spinlock_mutex
)
{
    using value_type = float;
    using mutex_type = TestType;
    using experimental_multithreading::static_stack;
    using benchmark_helpers::cpu_timer;
    static constexpr std::size_t threads_n = 10;
    static constexpr std::size_t elements_per_thread = 10;
    static constexpr std::size_t stack_max_size = threads_n*elements_per_thread;

    std::array<std::thread, threads_n> workers{};
    static_stack<value_type, stack_max_size, mutex_type> stack{};
    for(std::size_t i{0}; i!=stack_max_size; ++i){
        stack.push(static_cast<value_type>(i));
    }
    REQUIRE(stack.size() == stack_max_size);
    auto f = [&stack](){
        for(std::size_t i{0}; i!=elements_per_thread; ++i){
            auto v = stack.pop();
            //std::this_thread::sleep_for(std::chrono::milliseconds{i%13});
            stack.push(v);
        }
    };
    for(int i{0}; i!=threads_n; ++i){
        workers[i] = std::thread(f);
    }
    cpu_timer start{};
    for(int i{0}; i!=threads_n; ++i){
        workers[i].join();
    }
    cpu_timer stop{};
    auto dt_ms = stop - start;
    std::cout<<std::endl<<"dt_ms "<<dt_ms;
}