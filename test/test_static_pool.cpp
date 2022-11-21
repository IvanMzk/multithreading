#include <thread>
#include <iostream>
#include <array>
#include <chrono>
#include <tuple>
#include <set>
#include "catch.hpp"
#include "static_pool.hpp"

namespace test_static_pool{

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

}   //end of namespace test_static_pool

TEST_CASE("test_default_constructor","[test_static_pool]")
{
    using mutex_type = std::mutex;
    using value_type = float;
    using experimental_multithreading::static_pool;
    static constexpr std::size_t pool_size = 10;

    static_pool<value_type, pool_size, mutex_type> pool{};
    REQUIRE(pool.size() == pool_size);
    REQUIRE(*pool.pop().get() == 0.0f);
}

TEST_CASE("test_value_constructor","[test_static_pool]")
{
    using mutex_type = std::mutex;
    using value_type = float;
    using experimental_multithreading::static_pool;
    static constexpr std::size_t pool_size = 10;
    value_type v{1.0f};
    static_pool<value_type, pool_size, mutex_type> pool{v};
    REQUIRE(pool.size() == pool_size);
    REQUIRE(*pool.pop().get() == v);
}

TEST_CASE("test_pop","[test_static_pool]")
{
    using mutex_type = std::mutex;
    using value_type = float;
    using experimental_multithreading::static_pool;
    static constexpr std::size_t pool_size = 10;

    static_pool<value_type, pool_size, mutex_type> pool{};
    {
        auto e = pool.pop();
        REQUIRE(e.use_count() == 1);
        REQUIRE(pool.size() == pool_size-1);
    }
    REQUIRE(pool.size() == pool_size);
    {
        auto e = pool.pop();
        auto e1 = pool.pop();
        auto e2 = pool.pop();
        REQUIRE(e.use_count() == 1);
        REQUIRE(e1.use_count() == 1);
        REQUIRE(e2.use_count() == 1);
        REQUIRE(pool.size() == pool_size-3);
    }
    REQUIRE(pool.size() == pool_size);
    {
        std::vector<decltype(pool.pop())> v{};
        for (std::size_t i{0}; i!=pool_size; ++i){
            v.push_back(pool.pop());
        }
        REQUIRE(pool.size() == 0);
        REQUIRE(!pool.pop());
    }
    REQUIRE(pool.size() == pool_size);
}

TEST_CASE("test_copy_constructor","[test_static_pool]")
{
    using mutex_type = std::mutex;
    using value_type = float;
    using experimental_multithreading::static_pool;
    static constexpr std::size_t pool_size = 10;

    static_pool<value_type, pool_size, mutex_type> pool{};
    {
        auto e = pool.pop();
        auto e_copy{e};
        REQUIRE(e.use_count() == 2);
        REQUIRE(e_copy.use_count() == 2);
        REQUIRE(pool.size() == pool_size-1);
    }
    REQUIRE(pool.size() == pool_size);
}

TEST_CASE("test_reset","[test_static_pool]")
{
    using mutex_type = std::mutex;
    using value_type = float;
    using experimental_multithreading::static_pool;
    static constexpr std::size_t pool_size = 10;

    static_pool<value_type, pool_size, mutex_type> pool{};
    auto e = pool.pop();
    REQUIRE(e.use_count() == 1);
    REQUIRE(pool.size() == pool_size-1);
    e.reset();
    REQUIRE(pool.size() == pool_size);
}

TEST_CASE("test_multithread","[test_static_pool]")
{
    using mutex_type = std::mutex;
    using value_type = float;
    using experimental_multithreading::static_pool;
    static constexpr std::size_t threads_n = 10;
    static constexpr std::size_t elements_per_thread = 10;
    static constexpr std::size_t pool_size = 100;

    static_pool<value_type, pool_size, mutex_type> pool(value_type(11));
    std::array<std::thread, threads_n> workers{};

    std::vector<decltype(pool.pop().get())> expected_elements_id{};
    {
        std::vector<decltype(pool.pop())> v{};
        while(pool.size()){
            auto e = pool.pop();
            expected_elements_id.push_back(e.get());
            v.push_back(e);
        }
    }
    REQUIRE(pool.size() == pool_size);
    auto f = [&pool](){
        for(std::size_t i{0}; i!=elements_per_thread; ++i){
            //auto e = pool.pop(false);
            auto e = pool.pop(true);
            if (e){
                //auto e_copy{e};
                std::this_thread::sleep_for(std::chrono::milliseconds{1});
                // if (i%2){
                //     e.reset();
                // }
                //std::cout<<std::endl<<"got element"<<e.get();
            }else{
                //std::cout<<std::endl<<"got empty element";
            }
        }
    };
    for(int i{0}; i!=threads_n; ++i){
        workers[i] = std::thread(f);
    }
    for(int i{0}; i!=threads_n; ++i){
        workers[i].join();
    }
    std::vector<decltype(pool.pop().get())> result_elements_id{};
    {
        std::vector<decltype(pool.pop())> v{};
        while(pool.size()){
            auto e = pool.pop();
            result_elements_id.push_back(e.get());
            v.push_back(e);
        }
    }
    using id_type = typename decltype(expected_elements_id)::value_type;
    REQUIRE(std::set<id_type>(expected_elements_id.begin(), expected_elements_id.end()).size()  == pool_size);
    REQUIRE(std::set<id_type>(expected_elements_id.begin(), expected_elements_id.end()) == std::set<id_type>(result_elements_id.begin(), result_elements_id.end()));
}

