#include <thread>
#include <iostream>
#include <array>
#include <chrono>
#include "catch.hpp"
#include "static_stack.hpp"

namespace test_static_stack{

struct test_value_type{
    std::size_t a;
    std::size_t b;
    std::size_t c;

    test_value_type() = default;
    test_value_type(std::size_t v):
        a{v},
        b{v},
        c{v}
    {}
    bool operator==(const test_value_type& other)const{
        return a==other.a && b==other.b && c==other.c;
    }
    friend std::ostream& operator<<(std::ostream& os, const test_value_type& lhs){
        return os<<"("<<lhs.a<<" "<<lhs.b<<" "<<lhs.c<<")";
    }
    auto get()const{return a;}
};

}


TEST_CASE("single_thread_test","[test_static_stack]"){
    using value_type = float;
    static constexpr std::size_t stack_max_size = 10;
    using experimental_multithreading::static_stack;

    static_stack<value_type, stack_max_size> stack{};
    REQUIRE(stack.size() == 0);
    for(int i{0}; i!=stack_max_size; ++i){
        stack.push(i);
    }
    REQUIRE(stack.size() == stack_max_size);
    for(int i{0}; i!=stack_max_size; ++i){
        REQUIRE(stack[i] == i);
    }
}

TEST_CASE("push_multi_thread_test","[test_static_stack]"){
    //using value_type = test_static_stack::test_value_type;
    using value_type = std::size_t;
    using experimental_multithreading::static_stack;
    static constexpr std::size_t threads_n = 10;
    static constexpr std::size_t pushes_per_thread = 10;
    static constexpr std::size_t stack_max_size = threads_n*pushes_per_thread;

    std::array<std::thread, threads_n> workers{};

    static_stack<value_type, stack_max_size> stack{};
    REQUIRE(stack.size() == 0);
    auto f = [&stack](std::size_t min, std::size_t max){
        for(std::size_t i{min}; i!=max; ++i){
            std::this_thread::sleep_for(std::chrono::milliseconds{i%13});
            stack.push(value_type{i});
        }
    };
    for(int i{0}; i!=threads_n; ++i){
        auto min = i*pushes_per_thread;
        auto max = min+pushes_per_thread;
        workers[i] = std::thread(f,min,max);
    }
    for(int i{0}; i!=threads_n; ++i){
        workers[i].join();
    }
    REQUIRE(stack.size() == stack_max_size);
    std::vector<value_type> expected(stack_max_size, -1);
    for(std::size_t i{0}; i!=stack_max_size; ++i){
        auto r = stack[i];
        std::cout<<std::endl<<r<<r;
        expected[r] = r;
        //std::cout<<std::endl<<r.get()<<r;
        //expected[r.get()] = r;
    }
    for(std::size_t i{0}; i!=stack_max_size; ++i){
        std::cout<<std::endl<<expected[i]<<value_type{i};
        REQUIRE(expected[i] == value_type{i});
    }

}