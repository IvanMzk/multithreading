#include <thread>
#include <iostream>
#include <array>
#include <chrono>
#include <tuple>
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

}   //end of namespace test_static_stack

TEST_CASE("test_default_constructor","[test_static_pool]")
{
    using mutex_type = std::mutex;
    using value_type = float;
    using experimental_multithreading::static_pool;
    static constexpr std::size_t pool_size = 10;

    static_pool<value_type, pool_size, mutex_type> pool{};
    REQUIRE(pool.size() == pool_size);
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
}