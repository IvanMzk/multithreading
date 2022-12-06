#include <array>
#include <iostream>
#include "catch.hpp"
#include "benchmark_helpers.hpp"
#include "circular_buffer.hpp"


TEST_CASE("test_make_ranges","test_make_ranges"){
    using benchmark_helpers::make_ranges;

    REQUIRE(make_ranges<10,1>() == std::array<std::size_t,2>{0,10});
    REQUIRE(make_ranges<10,2>() == std::array<std::size_t,3>{0,5,10});
    REQUIRE(make_ranges<10,3>() == std::array<std::size_t,4>{0,3,6,10});
    REQUIRE(make_ranges<101,5>() == std::array<std::size_t,6>{0,20,40,60,80,101});
}

namespace benchmark_mpmc_circular_buffer{
    using value_type = float;
    static constexpr std::size_t n_elements = 100*1024*1024;
    static constexpr std::size_t buffer_size = 64;
    //static constexpr std::size_t buffer_size = 256;
}

TEMPLATE_TEST_CASE("benchmark_mpmc_circular_buffer","[benchmark_mpmc_circular_buffer]",
    (experimental_multithreading::mpmc_circular_buffer<benchmark_mpmc_circular_buffer::value_type, benchmark_mpmc_circular_buffer::buffer_size>),
    (experimental_multithreading::mpmc_lock_free_circular_buffer_v2<benchmark_mpmc_circular_buffer::value_type, benchmark_mpmc_circular_buffer::buffer_size>),
    (experimental_multithreading::mpmc_lock_free_circular_buffer_v3<benchmark_mpmc_circular_buffer::value_type, benchmark_mpmc_circular_buffer::buffer_size>)
)
{
    using benchmark_helpers::make_ranges;
    using benchmark_helpers::cpu_timer;

    using buffer_type = TestType;
    using value_type = typename buffer_type::value_type;
    static constexpr std::size_t buffer_size = typename buffer_type::buffer_size;
    static constexpr std::size_t n_producers = 10;
    static constexpr std::size_t n_consumers = 10;
    static constexpr std::size_t n_elements = benchmark_mpmc_circular_buffer::n_elements;

    buffer_type buffer{};
    std::vector<value_type> expected(n_elements);
    for (std::size_t i{0}; i!=n_elements; ++i){
        expected[i] = static_cast<value_type>(i);
    }
    std::atomic<std::size_t> producer_counter{0};
    auto producer_f = [&buffer,&producer_counter](auto producer_id, auto first, auto last){
        std::for_each(first,last,
            [&producer_id,&buffer,&producer_counter](const auto& v){
                std::size_t i{0};
                while(!buffer.try_push(v)){}
                producer_counter.fetch_add(1);
            }
        );
    };

    std::atomic<std::size_t> consumer_counter{0};
    auto consumer_f = [&buffer,&consumer_counter](auto first, auto last){
        std::for_each(first,last,
            [&buffer,&consumer_counter](auto& v){
                while(!buffer.try_pop(v)){};
                consumer_counter.fetch_add(1);
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
        *producers_it = std::thread(producer_f, id, expected.begin()+*it , expected.begin()+*(it+1));
    }
    for(auto it = consumer_ranges.begin(); it!=consumer_ranges.end()-1; ++it,++consumers_it){
        *consumers_it = std::thread(consumer_f, result.begin()+*it , result.begin()+*(it+1));
    }

    std::cout<<std::endl<<"join_producers...";
    std::for_each(producers.begin(),producers.end(),[](auto& t){t.join();});
    std::cout<<std::endl<<"producers_counter"<<producer_counter.load();
    std::cout<<std::endl<<"join...consumers";
    std::for_each(consumers.begin(),consumers.end(),[](auto& t){t.join();});
    std::cout<<std::endl<<"consumers_counter"<<consumer_counter.load();
    auto stop = cpu_timer{};

    std::cout<<std::endl<<typeid(buffer_type).name()<<"data transfer, ms"<<stop-start;

    std::sort(result.begin(),result.end());
    REQUIRE(result.size() == expected.size());
    REQUIRE(result == expected);
    REQUIRE(buffer.size() == 0);
}