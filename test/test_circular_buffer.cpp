
#include <thread>
#include <vector>
#include <set>
#include <iostream>
#include "catch.hpp"
#include "benchmark_helpers.hpp"
#include "circular_buffer.hpp"
#include "four_pointers_circular_buffer.hpp"

TEMPLATE_TEST_CASE("test_circular_buffer_empty","[test_circular_buffer]",
    (experimental_multithreading::spsc_circular_buffer<float, 10>),
    (experimental_multithreading::mpmc_circular_buffer<float, 10>),
    (experimental_multithreading::mpmc_lock_free_circular_buffer_v1<float, 10>),
    (experimental_multithreading::mpmc_lock_free_circular_buffer_v2<float, 10>),
    (experimental_multithreading::mpmc_lock_free_circular_buffer_v3<float, 10>)
){
    using buffer_type = TestType;
    using value_type = typename buffer_type::value_type;
    using experimental_multithreading::spsc_circular_buffer;
    static constexpr std::size_t buffer_size = buffer_type::buffer_size;

    buffer_type buffer{};

    value_type v_{11};
    value_type v{v_};
    REQUIRE(!buffer.try_pop(v));
    REQUIRE(v == v_);

    value_type v_to_push;
    REQUIRE(buffer.try_push(v_to_push));
    REQUIRE(buffer.try_pop(v));
    REQUIRE(v == v_to_push);

    REQUIRE(!buffer.try_pop(v));
    REQUIRE(v == v_to_push);
}

TEMPLATE_TEST_CASE("test_circular_buffer_full","[test_circular_buffer]",
    (experimental_multithreading::spsc_circular_buffer<float, 10>),
    (experimental_multithreading::mpmc_circular_buffer<float, 10>),
    (experimental_multithreading::mpmc_lock_free_circular_buffer_v1<float, 10>),
    (experimental_multithreading::mpmc_lock_free_circular_buffer_v2<float, 10>),
    (experimental_multithreading::mpmc_lock_free_circular_buffer_v3<float, 10>)
){
    using buffer_type = TestType;
    using value_type = typename buffer_type::value_type;
    using experimental_multithreading::spsc_circular_buffer;
    static constexpr std::size_t buffer_size = buffer_type::buffer_size;

    buffer_type buffer{};

    value_type v_{11};
    std::size_t i{0};
    while(i!=buffer_size){
        if (!buffer.try_push(v_)){
            break;
        }
        ++i;
    }
    REQUIRE(i == buffer_size);
    REQUIRE(buffer.size() == buffer_size);
    REQUIRE(!buffer.try_push(value_type{}));

    value_type v{};
    i=0;
    while(i!=buffer_size){
        if (!buffer.try_pop(v)){
            break;
        }
        ++i;
    }
    REQUIRE(i == buffer_size);
    REQUIRE(buffer.size() == 0);
    REQUIRE(!buffer.try_pop(v));
}

TEMPLATE_TEST_CASE("test_circular_buffer_overwrite","[test_circular_buffer]",
    (experimental_multithreading::spsc_circular_buffer<float, 10>),
    (experimental_multithreading::mpmc_circular_buffer<float, 10>),
    (experimental_multithreading::mpmc_lock_free_circular_buffer_v1<float, 10>),
    (experimental_multithreading::mpmc_lock_free_circular_buffer_v2<float, 10>),
    (experimental_multithreading::mpmc_lock_free_circular_buffer_v3<float, 10>)
){
    using buffer_type = TestType;
    using value_type = typename buffer_type::value_type;
    using experimental_multithreading::spsc_circular_buffer;
    static constexpr std::size_t buffer_size = buffer_type::buffer_size;

    buffer_type buffer{};

    value_type v_{11};
    std::size_t i{0};
    while(i!=buffer_size){
        if (!buffer.try_push(v_)){
            break;
        }
        ++i;
    }
    value_type v{};
    i=0;
    while(i!=buffer_size){
        if (!buffer.try_pop(v)){
            break;
        }
        ++i;
    }
    i=0;
    while(i!=buffer_size){
        if (!buffer.try_push(v_)){
            break;
        }
        ++i;
    }
    REQUIRE(i == buffer_size);
    REQUIRE(buffer.size() == buffer_size);
    REQUIRE(!buffer.try_push(value_type{}));
}

TEST_CASE("test_spsc_circular_buffer_multithread","[test_circular_buffer]"){
    using value_type = float;
    using experimental_multithreading::spsc_circular_buffer;
    static constexpr std::size_t buffer_size = 703;
    static constexpr std::size_t n_elements = 1024*1024;

    spsc_circular_buffer<value_type, buffer_size> buffer{};
    std::vector<value_type> expected(n_elements);
    for (std::size_t i{0}; i!=n_elements; ++i){
        expected[i] = static_cast<value_type>(i);
    }
    auto producer_f = [&expected,&buffer](){
        std::size_t i{0};
        while (i!=expected.size()){
            if (buffer.try_push(expected[i])){
                ++i;
            }
        }
    };
    std::vector<value_type> result(n_elements);
    auto consumer_f = [&result,&buffer](){
        std::size_t i{0};
        while (i!=result.size()){
            if (buffer.try_pop(result[i])){
                ++i;
            }
        }
    };
    auto producer = std::thread(producer_f);
    auto consumer = std::thread(consumer_f);

    producer.join();
    consumer.join();

    REQUIRE(result == expected);
    REQUIRE(buffer.size() == 0);
}


namespace test_mpmc_circular_buffer_try_push_try_pop_multithread{
    using value_type = float;
    static constexpr std::size_t n_elements = 100*1024*1024;
    static constexpr std::size_t buffer_size = 64;
    //static constexpr std::size_t buffer_size = 256;
}
TEMPLATE_TEST_CASE("test_mpmc_circular_buffer_try_push_try_pop_multithread","[test_circular_buffer]",
    //(experimental_multithreading::mpmc_lock_free_circular_buffer_v2<test_mpmc_circular_buffer_try_push_try_pop_multithread::value_type, test_mpmc_circular_buffer_try_push_try_pop_multithread::buffer_size, std::uint8_t>)
    //(experimental_multithreading::mpmc_lock_free_circular_buffer_v2<test_mpmc_circular_buffer_try_push_try_pop_multithread::value_type, test_mpmc_circular_buffer_try_push_try_pop_multithread::buffer_size, std::uint16_t>)
    //(experimental_multithreading::mpmc_lock_free_circular_buffer_v2_cas_loop_aligned_counters_elements_acq_rel<test_mpmc_circular_buffer_try_push_try_pop_multithread::value_type, test_mpmc_circular_buffer_try_push_try_pop_multithread::buffer_size>),
    (experimental_multithreading::mpmc_lock_free_circular_buffer_v3_aligned_counters<test_mpmc_circular_buffer_try_push_try_pop_multithread::value_type, test_mpmc_circular_buffer_try_push_try_pop_multithread::buffer_size>)
)
{
    using benchmark_helpers::make_ranges;
    using buffer_type = TestType;
    using value_type = typename buffer_type::value_type;
    static constexpr std::size_t buffer_size = typename buffer_type::buffer_size;
    static constexpr std::size_t n_elements = test_mpmc_circular_buffer_try_push_try_pop_multithread::n_elements;
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
                while(!buffer.try_push(v)){
                    // if(++i == 1024*1024){
                    //     std::cout<<std::endl<<"producer"<<producer_id<<"...";
                    //     i = 0;
                    // }
                }
                producer_counter.fetch_add(1);
            }
        );
        //std::cout<<std::endl<<"producer"<<producer_id<<"complete";
    };

    std::atomic<std::size_t> consumer_counter{0};
    auto consumer_f = [&buffer,&consumer_counter](auto first, auto last){
        std::for_each(first,last,
            [&buffer,&consumer_counter](auto& v){
                while(!buffer.try_pop(v)){
                    //std::cout<<std::endl<<"consumer_f";
                }
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


    // std::this_thread::sleep_for(std::chrono::milliseconds{10000});
    // buffer.debug_stop();
    // std::cout<<std::endl<<"producers_counter"<<producer_counter.load();
    // std::cout<<std::endl<<"consumers_counter"<<consumer_counter.load();
    // std::this_thread::sleep_for(std::chrono::milliseconds{1000});
    // std::cout<<std::endl<<buffer.debug_to_str();

    // buffer.debug_start();
    // std::this_thread::sleep_for(std::chrono::milliseconds{5000});
    // std::cout<<std::endl<<"producers_counter"<<producer_counter.load();
    // std::cout<<std::endl<<"consumers_counter"<<consumer_counter.load();
    // std::this_thread::sleep_for(std::chrono::milliseconds{1000});
    // std::cout<<std::endl<<buffer.debug_to_str();

    // buffer.debug_start();
    // std::this_thread::sleep_for(std::chrono::milliseconds{5000});
    // std::cout<<std::endl<<"producers_counter"<<producer_counter.load();
    // std::cout<<std::endl<<"consumers_counter"<<consumer_counter.load();
    // std::this_thread::sleep_for(std::chrono::milliseconds{1000});
    // std::cout<<std::endl<<buffer.debug_to_str();

    // buffer.debug_start();
    // std::this_thread::sleep_for(std::chrono::milliseconds{5000});
    // std::cout<<std::endl<<"producers_counter"<<producer_counter.load();
    // std::cout<<std::endl<<"consumers_counter"<<consumer_counter.load();
    // std::this_thread::sleep_for(std::chrono::milliseconds{1000});
    // std::cout<<std::endl<<buffer.debug_to_str();


    std::cout<<std::endl<<"join_producers...";
    std::for_each(producers.begin(),producers.end(),[](auto& t){t.join();});
    std::cout<<std::endl<<"producers_counter"<<producer_counter.load();


    std::cout<<std::endl<<"join...consumers";
    std::for_each(consumers.begin(),consumers.end(),[](auto& t){t.join();});
    std::cout<<std::endl<<"consumers_counter"<<consumer_counter.load();

    std::sort(result.begin(),result.end());

    REQUIRE(result.size() == expected.size());
    REQUIRE(result == expected);
    REQUIRE(buffer.size() == 0);
}


namespace test_circular_buffer_push_pop_multithread{
    using value_type = float;
    static constexpr std::size_t n_elements = 100*1024*1024;
    static constexpr std::size_t buffer_size = 64;
    //static constexpr std::size_t buffer_size = 256;
}
TEMPLATE_TEST_CASE("test_circular_buffer_push_pop_multithread","[test_circular_buffer]",
    //(experimental_multithreading::mpmc_lock_free_circular_buffer_v2_cas_loop_aligned_counters_elements_acq_rel<test_circular_buffer_push_pop_multithread::value_type, test_circular_buffer_push_pop_multithread::buffer_size>)
    (experimental_multithreading::mpmc_lock_free_circular_buffer_v3_aligned_counters<test_circular_buffer_push_pop_multithread::value_type, test_circular_buffer_push_pop_multithread::buffer_size>)
)
{
    using benchmark_helpers::make_ranges;
    using buffer_type = TestType;
    using value_type = typename buffer_type::value_type;
    static constexpr std::size_t buffer_size = typename buffer_type::buffer_size;
    static constexpr std::size_t n_elements = test_circular_buffer_push_pop_multithread::n_elements;
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
                buffer.push(v);
                producer_counter.fetch_add(1);
            }
        );
    };

    std::atomic<std::size_t> consumer_counter{0};
    auto consumer_f = [&buffer,&consumer_counter](auto first, auto last){
        std::for_each(first,last,
            [&buffer,&consumer_counter](auto& v){
                buffer.pop(v);
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

    std::cout<<std::endl<<"join_producers...";
    std::for_each(producers.begin(),producers.end(),[](auto& t){t.join();});
    std::cout<<std::endl<<"producers_counter"<<producer_counter.load();


    std::cout<<std::endl<<"join...consumers";
    std::for_each(consumers.begin(),consumers.end(),[](auto& t){t.join();});
    std::cout<<std::endl<<"consumers_counter"<<consumer_counter.load();

    std::sort(result.begin(),result.end());

    REQUIRE(result.size() == expected.size());
    REQUIRE(result == expected);
    REQUIRE(buffer.size() == 0);
}

