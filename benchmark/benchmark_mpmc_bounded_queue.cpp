#include <array>
#include <thread>
#include <iostream>

#include "catch.hpp"
#include "benchmark_helpers.hpp"
#include "queue.hpp"


TEST_CASE("test_make_ranges","test_make_ranges"){
    using benchmark_helpers::make_ranges;

    REQUIRE(make_ranges<10,1>() == std::array<std::size_t,2>{0,10});
    REQUIRE(make_ranges<10,2>() == std::array<std::size_t,3>{0,5,10});
    REQUIRE(make_ranges<10,3>() == std::array<std::size_t,4>{0,3,6,10});
    REQUIRE(make_ranges<101,5>() == std::array<std::size_t,6>{0,20,40,60,80,101});
}

namespace benchmark_mpmc_bounded_queue{
    using value_type = float;
    static constexpr std::size_t n_elements = 10*1000*1000;
    static constexpr std::size_t capacity = 64;
}

TEMPLATE_TEST_CASE("benchmark_mpmc_bounded_queue_not_blocking_interface","[benchmark_mpmc_bounded_queue]",
    (queue::mpmc_bounded_queue_v1<benchmark_mpmc_bounded_queue::value_type>),
    (queue::mpmc_bounded_queue_v2<benchmark_mpmc_bounded_queue::value_type>),
    (queue::mpmc_bounded_queue_v3<benchmark_mpmc_bounded_queue::value_type>)
)
{
    using benchmark_helpers::make_ranges;
    using benchmark_helpers::cpu_timer;

    using queue_type = TestType;
    using value_type = typename queue_type::value_type;
    static constexpr std::size_t n_producers = 10;
    static constexpr std::size_t n_consumers = 10;
    static constexpr std::size_t n_elements = benchmark_mpmc_bounded_queue::n_elements;

    queue_type queue{benchmark_mpmc_bounded_queue::capacity};
    std::vector<value_type> expected(n_elements);
    for (std::size_t i{0}; i!=n_elements; ++i){
        expected[i] = static_cast<value_type>(i);
    }
    auto producer_f = [&queue](auto first, auto last){
        std::for_each(first,last,
            [&queue](const auto& v){
                while(!queue.try_push(v)){
                    std::this_thread::yield();
                }
            }
        );
    };
    auto consumer_f = [&queue](auto first, auto last){
        std::for_each(first,last,
            [&queue](auto& v){
                while(!queue.try_pop(v)){
                    std::this_thread::yield();
                };
            }
        );
    };
    std::array<std::thread, n_producers> producers;
    std::array<std::thread, n_consumers> consumers;

    static constexpr auto producer_ranges = make_ranges<n_elements,n_producers>();
    static constexpr auto consumer_ranges = make_ranges<n_elements,n_consumers>();
    std::size_t id{0};
    std::vector<value_type> result(n_elements);
    auto producers_it = producers.begin();
    auto consumers_it = consumers.begin();

    auto start = cpu_timer{};
    for(auto it = producer_ranges.begin(); it!=producer_ranges.end()-1; ++it,++producers_it,++id){
        *producers_it = std::thread(producer_f, expected.begin()+*it , expected.begin()+*(it+1));
    }
    for(auto it = consumer_ranges.begin(); it!=consumer_ranges.end()-1; ++it,++consumers_it){
        *consumers_it = std::thread(consumer_f, result.begin()+*it , result.begin()+*(it+1));
    }

    std::for_each(producers.begin(),producers.end(),[](auto& t){t.join();});
    std::for_each(consumers.begin(),consumers.end(),[](auto& t){t.join();});
    auto stop = cpu_timer{};

    std::cout<<std::endl<<typeid(queue_type).name()<<" non blocking data transfer, ms "<<stop-start;

    std::sort(result.begin(),result.end());
    REQUIRE(result.size() == expected.size());
    REQUIRE(result == expected);
    REQUIRE(queue.size() == 0);
}

TEMPLATE_TEST_CASE("benchmark_mpmc_bounded_queue_blocking_interface","[benchmark_mpmc_bounded_queue]",
    (queue::mpmc_bounded_queue_v1<benchmark_mpmc_bounded_queue::value_type>),
    (queue::mpmc_bounded_queue_v2<benchmark_mpmc_bounded_queue::value_type>),
    (queue::mpmc_bounded_queue_v3<benchmark_mpmc_bounded_queue::value_type>)
)
{
    using benchmark_helpers::make_ranges;
    using benchmark_helpers::cpu_timer;

    using queue_type = TestType;
    using value_type = typename queue_type::value_type;
    static constexpr std::size_t n_producers = 10;
    static constexpr std::size_t n_consumers = 10;
    static constexpr std::size_t n_elements = benchmark_mpmc_bounded_queue::n_elements;

    queue_type queue{benchmark_mpmc_bounded_queue::capacity};
    std::vector<value_type> expected(n_elements);
    for (std::size_t i{0}; i!=n_elements; ++i){
        expected[i] = static_cast<value_type>(i);
    }
    auto producer_f = [&queue](auto first, auto last){
        std::for_each(first,last,
            [&queue](const auto& v){
                queue.push(v);
            }
        );
    };

    auto consumer_f = [&queue](auto first, auto last){
        std::for_each(first,last,
            [&queue](auto& v){
                queue.pop(v);
            }
        );
    };
    std::array<std::thread, n_producers> producers;
    std::array<std::thread, n_consumers> consumers;

    static constexpr auto producer_ranges = make_ranges<n_elements,n_producers>();
    static constexpr auto consumer_ranges = make_ranges<n_elements,n_consumers>();
    std::size_t id{0};
    std::vector<value_type> result(n_elements);
    auto producers_it = producers.begin();
    auto consumers_it = consumers.begin();

    auto start = cpu_timer{};
    for(auto it = producer_ranges.begin(); it!=producer_ranges.end()-1; ++it,++producers_it,++id){
        *producers_it = std::thread(producer_f, expected.begin()+*it , expected.begin()+*(it+1));
    }
    for(auto it = consumer_ranges.begin(); it!=consumer_ranges.end()-1; ++it,++consumers_it){
        *consumers_it = std::thread(consumer_f, result.begin()+*it , result.begin()+*(it+1));
    }

    std::for_each(producers.begin(),producers.end(),[](auto& t){t.join();});
    std::for_each(consumers.begin(),consumers.end(),[](auto& t){t.join();});
    auto stop = cpu_timer{};

    std::cout<<std::endl<<typeid(queue_type).name()<<" blocking data transfer, ms "<<stop-start;

    std::sort(result.begin(),result.end());
    REQUIRE(result.size() == expected.size());
    REQUIRE(result == expected);
    REQUIRE(queue.size() == 0);
}