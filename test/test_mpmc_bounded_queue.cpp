
#include <thread>
#include <vector>
#include <set>
#include <iostream>
#include "catch.hpp"
#include "benchmark_helpers.hpp"
#include "mpmc_bounded_queue.hpp"

namespace test_mpmc_bounded_queue_single_thread{
    using value_type = float;
    static constexpr std::size_t capacity = 64;

    class constructor_destructor_counter{

        static std::size_t constructor_counter_;
        static std::size_t destructor_counter_;
    public:
        ~constructor_destructor_counter(){
            ++destructor_counter_;
        }
        constructor_destructor_counter(){
            ++constructor_counter_;
        }
        static auto constructor_counter(){return constructor_counter_;}
        static auto destructor_counter(){return destructor_counter_;}
        static void reset_constructor_counter(){constructor_counter_ = 0;}
        static void reset_destructor_counter(){destructor_counter_ = 0;}

    };

    std::size_t constructor_destructor_counter::constructor_counter_ = 0;
    std::size_t constructor_destructor_counter::destructor_counter_ = 0;
}
TEMPLATE_TEST_CASE("test_mpmc_bounded_queue_empty","[test_mpmc_bounded_queue]",
    (mpmc_bounded_queue::mpmc_bounded_queue_v1<test_mpmc_bounded_queue_single_thread::value_type, test_mpmc_bounded_queue_single_thread::capacity>),
    (mpmc_bounded_queue::mpmc_bounded_queue_v2<test_mpmc_bounded_queue_single_thread::value_type, test_mpmc_bounded_queue_single_thread::capacity>),
    (mpmc_bounded_queue::mpmc_bounded_queue_v3<test_mpmc_bounded_queue_single_thread::value_type, test_mpmc_bounded_queue_single_thread::capacity>)
){
    using queue_type = TestType;
    using value_type = typename queue_type::value_type;

    queue_type queue{};
    REQUIRE(queue.size() == 0);

    const value_type v_{11};
    const value_type v_to_push{v_+1};
    value_type v{v_};
    SECTION("try_push_try_pop"){
        REQUIRE(!queue.try_pop(v));
        REQUIRE(queue.size() == 0);
        REQUIRE(v == v_);

        REQUIRE(queue.try_push(v_to_push));
        REQUIRE(queue.size() == 1);
        REQUIRE(queue.try_pop(v));
        REQUIRE(queue.size() == 0);
        REQUIRE(v == v_to_push);

        REQUIRE(!queue.try_pop(v));
        REQUIRE(queue.size() == 0);
        REQUIRE(v == v_to_push);
    }
    SECTION("push_pop"){
        queue.push(v_to_push);
        REQUIRE(queue.size() == 1);
        queue.pop(v);
        REQUIRE(!queue.try_pop(v));
        REQUIRE(queue.size() == 0);
        REQUIRE(v == v_to_push);
    }
}

TEMPLATE_TEST_CASE("test_mpmc_bounded_queue_full","[test_mpmc_bounded_queue]",
    (mpmc_bounded_queue::mpmc_bounded_queue_v1<test_mpmc_bounded_queue_single_thread::value_type, test_mpmc_bounded_queue_single_thread::capacity>),
    (mpmc_bounded_queue::mpmc_bounded_queue_v2<test_mpmc_bounded_queue_single_thread::value_type, test_mpmc_bounded_queue_single_thread::capacity>),
    (mpmc_bounded_queue::mpmc_bounded_queue_v3<test_mpmc_bounded_queue_single_thread::value_type, test_mpmc_bounded_queue_single_thread::capacity>)
){
    using queue_type = TestType;
    using value_type = typename queue_type::value_type;

    queue_type queue{};
    const std::size_t capacity = queue.capacity();

    value_type v_{11};
    value_type v{};
    std::size_t i{0};
    SECTION("try_push_try_pop"){
        while(i!=capacity){
            if (!queue.try_push(v_)){
                break;
            }
            ++i;
        }
        REQUIRE(i == capacity);
        REQUIRE(queue.size() == capacity);
        REQUIRE(!queue.try_push(value_type{}));

        i=0;
        while(i!=capacity){
            if (!queue.try_pop(v)){
                break;
            }
            ++i;
        }
        REQUIRE(i == capacity);
        REQUIRE(queue.size() == 0);
        REQUIRE(!queue.try_pop(v));

        i=0;
        while(i!=capacity){
            if (!queue.try_push(v_)){
                break;
            }
            ++i;
        }
        REQUIRE(i == capacity);
        REQUIRE(queue.size() == capacity);
        REQUIRE(!queue.try_push(value_type{}));
    }
    SECTION("push_pop"){
        while(i!=capacity){
            queue.push(v_);
            ++i;
        }
        REQUIRE(queue.size() == capacity);
        REQUIRE(!queue.try_push(value_type{}));

        i=0;
        while(i!=capacity){
            queue.pop(v);
            ++i;
        }
        REQUIRE(queue.size() == 0);
        REQUIRE(!queue.try_pop(v));

        i=0;
        while(i!=capacity){
            queue.push(v_);
            ++i;
        }
        REQUIRE(queue.size() == capacity);
        REQUIRE(!queue.try_push(value_type{}));
    }
}

TEMPLATE_TEST_CASE("test_mpmc_bounded_queue_clear","[test_mpmc_bounded_queue]",
    (mpmc_bounded_queue::mpmc_bounded_queue_v1<test_mpmc_bounded_queue_single_thread::constructor_destructor_counter, test_mpmc_bounded_queue_single_thread::capacity>),
    (mpmc_bounded_queue::mpmc_bounded_queue_v2<test_mpmc_bounded_queue_single_thread::constructor_destructor_counter, test_mpmc_bounded_queue_single_thread::capacity>),
    (mpmc_bounded_queue::mpmc_bounded_queue_v3<test_mpmc_bounded_queue_single_thread::constructor_destructor_counter, test_mpmc_bounded_queue_single_thread::capacity>)
){
    using queue_type = TestType;
    using value_type = typename queue_type::value_type;
    const std::size_t capacity = queue_type{}.capacity();

    value_type::reset_constructor_counter();
    value_type::reset_destructor_counter();

    SECTION("full_queue_1"){
        {
            queue_type queue{};
            while(queue.try_push());
            REQUIRE(queue.size() == capacity);
        }
        REQUIRE (value_type::destructor_counter() == capacity);
    }
    SECTION("full_queue_2"){
        {
            queue_type queue{};
            {
                value_type v;
                while(queue.try_push());
                for(std::size_t i = 0; i!=capacity-1; ++i){
                    queue.try_pop(v);
                }
                while(queue.try_push());
                REQUIRE(queue.size() == capacity);
            }
            REQUIRE (value_type::destructor_counter() == 1);
        }
        REQUIRE (value_type::destructor_counter() == capacity + 1);
    }
    SECTION("empty_queue_1"){
        {
            queue_type queue{};
        }
        REQUIRE (value_type::destructor_counter() == 0);
        REQUIRE (value_type::constructor_counter() == 0);
    }
    SECTION("empty_queue_2"){
        {
            queue_type queue{};
            {
                value_type v;
                while(queue.try_push());
                while(queue.try_pop(v));
            }
            REQUIRE (value_type::destructor_counter() == 1);
            REQUIRE(queue.size() == 0);
        }
        REQUIRE (value_type::destructor_counter() == 1);
    }
}

namespace test_mpmc_bounded_queue_multithread{
    using value_type = float;
    static constexpr std::size_t n_elements = 100*1000*1000;
    static constexpr std::size_t capacity = 30;
}
TEMPLATE_TEST_CASE("test_mpmc_bounded_queue_try_multithread","[test_mpmc_bounded_queue]",
    (mpmc_bounded_queue::mpmc_bounded_queue_v1<test_mpmc_bounded_queue_multithread::value_type, test_mpmc_bounded_queue_multithread::capacity>),
    (mpmc_bounded_queue::mpmc_bounded_queue_v2<test_mpmc_bounded_queue_multithread::value_type, test_mpmc_bounded_queue_multithread::capacity>),
    (mpmc_bounded_queue::mpmc_bounded_queue_v3<test_mpmc_bounded_queue_multithread::value_type, test_mpmc_bounded_queue_multithread::capacity>)
)
{
    using benchmark_helpers::make_ranges;
    using queue_type = TestType;
    using value_type = typename queue_type::value_type;
    static constexpr std::size_t n_elements = test_mpmc_bounded_queue_multithread::n_elements;
    static constexpr std::size_t n_producers = 10;
    static constexpr std::size_t n_consumers = 10;
    queue_type queue{};

    std::vector<value_type> expected(n_elements);
    for (std::size_t i{0}; i!=n_elements; ++i){
        expected[i] = static_cast<value_type>(i);
    }
    std::atomic<std::size_t> producer_counter{0};
    auto producer_f = [&queue,&producer_counter](auto producer_id, auto first, auto last){
        std::for_each(first,last,
            [&producer_id,&queue,&producer_counter](const auto& v){
                std::size_t i{0};
                while(!queue.try_push(v)){}
                producer_counter.fetch_add(1);
            }
        );
    };

    std::atomic<std::size_t> consumer_counter{0};
    auto consumer_f = [&queue,&consumer_counter](auto first, auto last){
        std::for_each(first,last,
            [&queue,&consumer_counter](auto& v){
                while(!queue.try_pop(v)){}
                consumer_counter.fetch_add(1);
            }
        );
    };
    std::array<std::thread, n_producers> producers;
    std::array<std::thread, n_consumers> consumers;

    static constexpr auto producer_ranges = make_ranges<n_elements,n_producers>();
    auto producers_it = producers.begin();
    std::size_t id{0};
    for(auto it = producer_ranges.begin(); it!=producer_ranges.end()-1; ++it,++producers_it,++id){
        *producers_it = std::thread(producer_f, id, expected.begin()+*it , expected.begin()+*(it+1));
    }
    std::vector<value_type> result(n_elements);
    static constexpr auto consumer_ranges = make_ranges<n_elements,n_consumers>();
    auto consumers_it = consumers.begin();
    for(auto it = consumer_ranges.begin(); it!=consumer_ranges.end()-1; ++it,++consumers_it){
        *consumers_it = std::thread(consumer_f, result.begin()+*it , result.begin()+*(it+1));
    }

    std::for_each(producers.begin(),producers.end(),[](auto& t){t.join();});
    std::for_each(consumers.begin(),consumers.end(),[](auto& t){t.join();});
    REQUIRE(producer_counter.load() == n_elements);
    REQUIRE(consumer_counter.load() == n_elements);

    std::sort(result.begin(),result.end());
    REQUIRE(result.size() == expected.size());
    REQUIRE(result == expected);
    REQUIRE(queue.size() == 0);
}

TEMPLATE_TEST_CASE("test_mpmc_bounded_queue_multithread","[test_mpmc_bounded_queue]",
    (mpmc_bounded_queue::mpmc_bounded_queue_v1<test_mpmc_bounded_queue_multithread::value_type, test_mpmc_bounded_queue_multithread::capacity>),
    (mpmc_bounded_queue::mpmc_bounded_queue_v2<test_mpmc_bounded_queue_multithread::value_type, test_mpmc_bounded_queue_multithread::capacity>),
    (mpmc_bounded_queue::mpmc_bounded_queue_v3<test_mpmc_bounded_queue_multithread::value_type, test_mpmc_bounded_queue_multithread::capacity>)
)
{
    using benchmark_helpers::make_ranges;
    using queue_type = TestType;
    using value_type = typename queue_type::value_type;
    static constexpr std::size_t n_elements = test_mpmc_bounded_queue_multithread::n_elements;
    static constexpr std::size_t n_producers = 10;
    static constexpr std::size_t n_consumers = 10;

    queue_type queue{};
    std::vector<value_type> expected(n_elements);
    for (std::size_t i{0}; i!=n_elements; ++i){
        expected[i] = static_cast<value_type>(i);
    }
    std::atomic<std::size_t> producer_counter{0};
    auto producer_f = [&queue,&producer_counter](auto producer_id, auto first, auto last){
        std::for_each(first,last,
            [&producer_id,&queue,&producer_counter](const auto& v){
                std::size_t i{0};
                queue.push(v);
                producer_counter.fetch_add(1);
            }
        );
    };

    std::atomic<std::size_t> consumer_counter{0};
    auto consumer_f = [&queue,&consumer_counter](auto first, auto last){
        std::for_each(first,last,
            [&queue,&consumer_counter](auto& v){
                queue.pop(v);
                consumer_counter.fetch_add(1);
            }
        );
    };
    std::array<std::thread, n_producers> producers;
    std::array<std::thread, n_consumers> consumers;

    static constexpr auto producer_ranges = make_ranges<n_elements,n_producers>();
    auto producers_it = producers.begin();
    std::size_t id{0};
    for(auto it = producer_ranges.begin(); it!=producer_ranges.end()-1; ++it,++producers_it,++id){
        *producers_it = std::thread(producer_f, id, expected.begin()+*it , expected.begin()+*(it+1));
    }
    std::vector<value_type> result(n_elements);
    static constexpr auto consumer_ranges = make_ranges<n_elements,n_consumers>();
    auto consumers_it = consumers.begin();
    for(auto it = consumer_ranges.begin(); it!=consumer_ranges.end()-1; ++it,++consumers_it){
        *consumers_it = std::thread(consumer_f, result.begin()+*it , result.begin()+*(it+1));
    }

    std::for_each(producers.begin(),producers.end(),[](auto& t){t.join();});
    std::for_each(consumers.begin(),consumers.end(),[](auto& t){t.join();});
    REQUIRE(producer_counter.load() == n_elements);
    REQUIRE(consumer_counter.load() == n_elements);

    std::sort(result.begin(),result.end());
    REQUIRE(result.size() == expected.size());
    REQUIRE(result == expected);
    REQUIRE(queue.size() == 0);
}

