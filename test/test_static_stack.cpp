#include <thread>
#include <iostream>
#include <array>
#include <chrono>
#include <tuple>
#include "catch.hpp"
#include "static_stack.hpp"
#include "spinlock_mutex.hpp"

namespace test_static_stack{

struct test_value_type{
    std::size_t a;
    std::size_t b;
    std::size_t c;
    std::size_t d;
    std::size_t e;

    test_value_type() = default;
    explicit test_value_type(std::size_t v):
        a{v},
        b{v},
        c{v},
        d{v},
        e{v}
    {}
    bool operator==(const test_value_type& other)const{
        return a==other.a && b==other.b && c==other.c && d==other.d && e==other.e;
    }
    bool operator!=(const test_value_type& other)const{
        return !(*this==other);
    }
    friend std::ostream& operator<<(std::ostream& os, const test_value_type& lhs){
        return os<<"("<<lhs.a<<" "<<lhs.b<<" "<<lhs.c<<" "<<lhs.d<<" "<<lhs.e<<")";
    }
    operator std::size_t(){return a;}
};

}   //end of namespace test_static_stack

TEMPLATE_TEST_CASE("test_push_pop_multi_thread","[test_static_stack]",
    (std::tuple<std::mutex,test_static_stack::test_value_type>),
    (std::tuple<std::mutex,std::size_t>),
    (std::tuple<experimental_multithreading::spinlock_mutex,test_static_stack::test_value_type>),
    (std::tuple<experimental_multithreading::spinlock_mutex,std::size_t>)
)
{
    using mutex_type = std::tuple_element_t<0,TestType>;
    using value_type = std::tuple_element_t<1,TestType>;
    using experimental_multithreading::static_stack;
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
            std::this_thread::sleep_for(std::chrono::milliseconds{i%13});
            stack.push(v);
        }
    };
    for(int i{0}; i!=threads_n; ++i){
        workers[i] = std::thread(f);
    }
    for(int i{0}; i!=threads_n; ++i){
        workers[i].join();
    }

    REQUIRE(stack.size() == stack_max_size);
    std::vector<value_type> expected(stack_max_size, static_cast<value_type>(-1));
    while(stack.size()){
        auto v = stack.pop();
        expected[static_cast<std::size_t>(v)] = v;
    }
    for(std::size_t i_{0}; i_!=stack_max_size; ++i_){
        if (expected[i_] != static_cast<value_type>(i_)){
            REQUIRE(expected[i_] == static_cast<value_type>(i_));
        }
    }


}