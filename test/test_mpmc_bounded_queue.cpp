
#include <thread>
#include <vector>
#include <set>
#include <iostream>
#include "catch.hpp"
#include "benchmark_helpers.hpp"
#include "mpmc_bounded_queue.hpp"

namespace test_mpmc_bounded_queue_single_thread{
    using value_type = float;
    static constexpr std::size_t buffer_capacity = 64;
}

TEMPLATE_TEST_CASE("test_mpmc_bounded_queue_empty","[test_mpmc_bounded_queue]",
    (mpmc_bounded_queue::mpmc_bounded_queue_v1<test_mpmc_bounded_queue_single_thread::value_type, test_mpmc_bounded_queue_single_thread::buffer_capacity>)
){
    using buffer_type = TestType;
    using value_type = typename buffer_type::value_type;

    buffer_type buffer{};
    REQUIRE(buffer.size() == 0);

    const value_type v_{11};
    const value_type v_to_push{v_+1};
    value_type v{v_};
    SECTION("try_push_try_pop"){
        REQUIRE(!buffer.try_pop(v));
        REQUIRE(buffer.size() == 0);
        REQUIRE(v == v_);

        REQUIRE(buffer.try_push(v_to_push));
        REQUIRE(buffer.size() == 1);
        REQUIRE(buffer.try_pop(v));
        REQUIRE(buffer.size() == 0);
        REQUIRE(v == v_to_push);

        REQUIRE(!buffer.try_pop(v));
        REQUIRE(buffer.size() == 0);
        REQUIRE(v == v_to_push);
    }
    SECTION("push_pop"){
        buffer.push(v_to_push);
        REQUIRE(buffer.size() == 1);
        buffer.pop(v);
        REQUIRE(!buffer.try_pop(v));
        REQUIRE(buffer.size() == 0);
        REQUIRE(v == v_to_push);
    }
}

TEMPLATE_TEST_CASE("test_mpmc_bounded_queue_full","[test_mpmc_bounded_queue]",
    (mpmc_bounded_queue::mpmc_bounded_queue_v1<test_mpmc_bounded_queue_single_thread::value_type, test_mpmc_bounded_queue_single_thread::buffer_capacity>)
){
    using buffer_type = TestType;
    using value_type = typename buffer_type::value_type;

    buffer_type buffer{};
    const std::size_t buffer_capacity = buffer.capacity();

    value_type v_{11};
    value_type v{};
    std::size_t i{0};
    SECTION("try_push_try_pop"){
        while(i!=buffer_capacity){
            if (!buffer.try_push(v_)){
                break;
            }
            ++i;
        }
        REQUIRE(i == buffer_capacity);
        REQUIRE(buffer.size() == buffer_capacity);
        REQUIRE(!buffer.try_push(value_type{}));

        i=0;
        while(i!=buffer_capacity){
            if (!buffer.try_pop(v)){
                break;
            }
            ++i;
        }
        REQUIRE(i == buffer_capacity);
        REQUIRE(buffer.size() == 0);
        REQUIRE(!buffer.try_pop(v));

        i=0;
        while(i!=buffer_capacity){
            if (!buffer.try_push(v_)){
                break;
            }
            ++i;
        }
        REQUIRE(i == buffer_capacity);
        REQUIRE(buffer.size() == buffer_capacity);
        REQUIRE(!buffer.try_push(value_type{}));
    }
    SECTION("push_pop"){
        while(i!=buffer_capacity){
            buffer.push(v_);
            ++i;
        }
        REQUIRE(buffer.size() == buffer_capacity);
        REQUIRE(!buffer.try_push(value_type{}));

        i=0;
        while(i!=buffer_capacity){
            buffer.pop(v);
            ++i;
        }
        REQUIRE(buffer.size() == 0);
        REQUIRE(!buffer.try_pop(v));

        i=0;
        while(i!=buffer_capacity){
            buffer.push(v_);
            ++i;
        }
        REQUIRE(buffer.size() == buffer_capacity);
        REQUIRE(!buffer.try_push(value_type{}));
    }
}

namespace test_mpmc_bounded_queue_multithread{
    using value_type = float;
    static constexpr std::size_t n_elements = 100*1024*1024;
    static constexpr std::size_t buffer_capacity = 64;
}
TEMPLATE_TEST_CASE("test_mpmc_bounded_queue_multithread","[test_mpmc_bounded_queue]",
    (mpmc_bounded_queue::mpmc_bounded_queue_v1<test_mpmc_bounded_queue_multithread::value_type, test_mpmc_bounded_queue_multithread::buffer_capacity>)
)
{
    using benchmark_helpers::make_ranges;
    using buffer_type = TestType;
    using value_type = typename buffer_type::value_type;
    static constexpr std::size_t n_elements = test_mpmc_bounded_queue_multithread::n_elements;
    static constexpr std::size_t n_producers = 10;
    static constexpr std::size_t n_consumers = 10;
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
                while(!buffer.try_pop(v)){}
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
    REQUIRE(buffer.size() == 0);
}


// namespace test_circular_buffer_push_pop_multithread{
//     using value_type = float;
//     static constexpr std::size_t n_elements = 100*1024*1024;
//     static constexpr std::size_t buffer_size = 64;
//     //static constexpr std::size_t buffer_size = 256;
// }
// TEMPLATE_TEST_CASE("test_circular_buffer_push_pop_multithread","[test_circular_buffer]",
//     //(experimental_multithreading::mpmc_lock_free_circular_buffer_v2_cas_loop_aligned_counters_elements_acq_rel<test_circular_buffer_push_pop_multithread::value_type, test_circular_buffer_push_pop_multithread::buffer_size>)
//     (experimental_multithreading::mpmc_lock_free_circular_buffer_v3_aligned_counters<test_circular_buffer_push_pop_multithread::value_type, test_circular_buffer_push_pop_multithread::buffer_size>)
// )
// {
//     using benchmark_helpers::make_ranges;
//     using buffer_type = TestType;
//     using value_type = typename buffer_type::value_type;
//     static constexpr std::size_t buffer_size = typename buffer_type::buffer_size;
//     static constexpr std::size_t n_elements = test_circular_buffer_push_pop_multithread::n_elements;
//     static constexpr std::size_t n_producers = 10;
//     static constexpr std::size_t n_consumers = 10;

//     buffer_type buffer{};
//     std::vector<value_type> expected(n_elements);
//     for (std::size_t i{0}; i!=n_elements; ++i){
//         expected[i] = static_cast<value_type>(i);
//     }
//     std::atomic<std::size_t> producer_counter{0};
//     auto producer_f = [&buffer,&producer_counter](auto producer_id, auto first, auto last){
//         std::for_each(first,last,
//             [&producer_id,&buffer,&producer_counter](const auto& v){
//                 std::size_t i{0};
//                 buffer.push(v);
//                 producer_counter.fetch_add(1);
//             }
//         );
//     };

//     std::atomic<std::size_t> consumer_counter{0};
//     auto consumer_f = [&buffer,&consumer_counter](auto first, auto last){
//         std::for_each(first,last,
//             [&buffer,&consumer_counter](auto& v){
//                 buffer.pop(v);
//                 consumer_counter.fetch_add(1);
//             }
//         );
//     };
//     std::array<std::thread, n_producers> producers;
//     std::array<std::thread, n_consumers> consumers;

//     static constexpr auto producer_ranges = make_ranges<n_elements,n_producers>();
//     auto producers_it = producers.begin();
//     std::size_t id{0};
//     for(auto it = producer_ranges.begin(); it!=producer_ranges.end()-1; ++it,++producers_it,++id){
//         *producers_it = std::thread(producer_f, id, expected.begin()+*it , expected.begin()+*(it+1));
//     }
//     std::vector<value_type> result(n_elements);
//     static constexpr auto consumer_ranges = make_ranges<n_elements,n_consumers>();
//     auto consumers_it = consumers.begin();
//     for(auto it = consumer_ranges.begin(); it!=consumer_ranges.end()-1; ++it,++consumers_it){
//         *consumers_it = std::thread(consumer_f, result.begin()+*it , result.begin()+*(it+1));
//     }

//     std::cout<<std::endl<<"join_producers...";
//     std::for_each(producers.begin(),producers.end(),[](auto& t){t.join();});
//     std::cout<<std::endl<<"producers_counter"<<producer_counter.load();


//     std::cout<<std::endl<<"join...consumers";
//     std::for_each(consumers.begin(),consumers.end(),[](auto& t){t.join();});
//     std::cout<<std::endl<<"consumers_counter"<<consumer_counter.load();

//     std::sort(result.begin(),result.end());

//     REQUIRE(result.size() == expected.size());
//     REQUIRE(result == expected);
//     REQUIRE(buffer.size() == 0);
// }

