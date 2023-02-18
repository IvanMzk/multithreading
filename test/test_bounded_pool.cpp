#include <thread>
#include <iostream>
#include <array>
#include <chrono>
#include <tuple>
#include <set>
#include <numeric>
#include "catch.hpp"
#include "bounded_pool.hpp"
#include "benchmark_helpers.hpp"

namespace test_bounded_pool{

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

}   //end of namespace test_bounded_pool

TEST_CASE("test_capacity_constructor","[test_bounded_pool]")
{
    using value_type = float;
    using bounded_pool::mc_bounded_pool;
    static constexpr std::size_t pool_size = 10;

    mc_bounded_pool<value_type> pool{pool_size};
    REQUIRE(pool.size() == pool_size);
    REQUIRE(pool.pop().get() == 0.0f);
}

TEST_CASE("test_value_constructor","[test_bounded_pool]")
{
    using value_type = float;
    using bounded_pool::mc_bounded_pool;
    static constexpr std::size_t pool_size = 10;
    value_type v{1.0f};
    mc_bounded_pool<value_type> pool{pool_size, v};
    REQUIRE(pool.size() == pool_size);
    REQUIRE(pool.pop().get() == v);
}

TEST_CASE("test_range_constructor","[test_bounded_pool]")
{
    using value_type = float;
    using bounded_pool::mc_bounded_pool;
    static constexpr std::size_t pool_size = 10;
    std::vector<value_type> expected(pool_size);
    std::iota(expected.begin(),expected.end(), 0.0f);
    mc_bounded_pool<value_type> pool{expected.begin(), expected.end()};
    REQUIRE(pool.size() == pool_size);
    std::vector<value_type> result{};
    {
        std::vector<decltype(pool.try_pop())> result_elements{};
        while(auto e = pool.try_pop()){
            result_elements.push_back(e);
        }
        REQUIRE(pool.size() == 0);
        std::for_each(result_elements.begin(), result_elements.end(), [&result](const auto& e){result.push_back(e.get());});
    }
    REQUIRE(pool.size() == pool_size);
    REQUIRE(result == expected);
}

TEST_CASE("test_pop","[test_bounded_pool]")
{
    using value_type = float;
    using bounded_pool::mc_bounded_pool;
    static constexpr std::size_t pool_size = 10;

    mc_bounded_pool<value_type> pool{pool_size};
    {
        auto e = pool.try_pop();
        REQUIRE(e);
        REQUIRE(e.use_count() == 1);
        REQUIRE(pool.size() == pool_size-1);
    }
    REQUIRE(pool.size() == pool_size);
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
        REQUIRE(!pool.try_pop());
    }
    REQUIRE(pool.size() == pool_size);
}

TEST_CASE("test_copy_constructor","[test_bounded_pool]")
{
    using value_type = float;
    using bounded_pool::mc_bounded_pool;
    static constexpr std::size_t pool_size = 10;

    mc_bounded_pool<value_type> pool{pool_size};
    {
        auto e = pool.pop();
        auto e_copy{e};
        REQUIRE(e.use_count() == 2);
        REQUIRE(e_copy.use_count() == 2);
        REQUIRE(pool.size() == pool_size-1);
    }
    REQUIRE(pool.size() == pool_size);
}

TEST_CASE("test_reset","[test_bounded_pool]")
{
    using value_type = float;
    using bounded_pool::mc_bounded_pool;
    static constexpr std::size_t pool_size = 10;

    mc_bounded_pool<value_type> pool{pool_size};
    auto e = pool.pop();
    REQUIRE(e.use_count() == 1);
    REQUIRE(pool.size() == pool_size-1);
    e.reset();
    REQUIRE(pool.size() == pool_size);
}

namespace test_mc_bounded_pool_multithread{

struct consumer{
    template<typename U, typename V>
    void operator()(std::reference_wrapper<U> pool, V first, V last){
        auto try_pop = [&pool](auto& v){
            while(true){
                if (auto e = pool.get().try_pop()){
                    v = e;
                    break;
                }
            }
        };
        auto pop = [&pool](auto& v){v = pool.get().pop();};
        std::size_t i{0};
        std::for_each(first,last,[&](auto& v){
            switch (i%2){
                case 0: try_pop(v); break;
                case 1: pop(v); break;
            }
            ++i;
        });
    }
};

}   //end of namespace test_mc_bounded_pool_multithread

TEST_CASE("test_multithread","[test_bounded_pool]")
{
    using value_type = float;
    using bounded_pool::mc_bounded_pool;
    using benchmark_helpers::make_ranges;
    using consumer_type = test_mc_bounded_pool_multithread::consumer;
    using pool_type = mc_bounded_pool<value_type>;
    static constexpr std::size_t n_consumers = 10;
    static constexpr std::size_t pool_size = 1*1000*1000;

    std::vector<value_type> expected(pool_size);
    std::iota(expected.begin(),expected.end(), 0.0f);
    pool_type pool{expected.begin(), expected.end()};

    std::array<std::thread, n_consumers> consumers{};
    std::vector<decltype(pool.pop())> result_elements{pool_size};

    static constexpr auto consumer_ranges = make_ranges<pool_size,n_consumers>();
    auto consumers_it = consumers.begin();
    for(auto it = consumer_ranges.begin(); it!=consumer_ranges.end()-1; ++it,++consumers_it){
        *consumers_it = std::thread(consumer_type{}, std::reference_wrapper<pool_type>{pool}, result_elements.begin()+*it , result_elements.begin()+*(it+1));
    }
    std::for_each(consumers.begin(),consumers.end(),[](auto& t){t.join();});
    REQUIRE(pool.size() == 0);
    std::vector<value_type> result{};
    std::for_each(result_elements.begin(), result_elements.end(), [&result](const auto& e){result.push_back(e.get());});
    result_elements.clear();
    REQUIRE(pool.size() == pool_size);
    std::sort(result.begin(),result.end());
    REQUIRE(result == expected);
}
