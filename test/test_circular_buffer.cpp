
#include <thread>
#include <vector>
#include <set>
#include <iostream>
#include "catch.hpp"
#include "circular_buffer.hpp"
#include "four_pointers_circular_buffer.hpp"

namespace test_circular_buffer{


template<std::size_t N, std::size_t N_groups>
struct make_ranges_helper{
    template<std::size_t...I>
    constexpr static auto make_ranges(std::index_sequence<I...>){return std::array<std::size_t,sizeof...(I)>{make_range<I>()...};}
    template<std::size_t I>
    constexpr static auto make_range(){return I*(N/N_groups);}
    template<>
    constexpr static auto make_range<N_groups>(){return N_groups*(N/N_groups)+(N%N_groups);}
};

template<std::size_t N, std::size_t N_groups>
constexpr auto make_ranges(){
    return make_ranges_helper<N,N_groups>::make_ranges(std::make_index_sequence<N_groups+1>{});
}

template<typename T, T...I>
auto test_integer_sequence(std::integer_sequence<T,I...> s){
    std::cout<<std::endl<<sizeof...(I)<<s.size();
}


}   //end of namespace test_circular_buffer

TEST_CASE("test_make_ranges","test_make_ranges"){
    using test_circular_buffer::make_ranges;

    REQUIRE(make_ranges<10,1>() == std::array<std::size_t,2>{0,10});
    REQUIRE(make_ranges<10,2>() == std::array<std::size_t,3>{0,5,10});
    REQUIRE(make_ranges<10,3>() == std::array<std::size_t,4>{0,3,6,10});
    REQUIRE(make_ranges<101,5>() == std::array<std::size_t,6>{0,20,40,60,80,101});
}


TEMPLATE_TEST_CASE("test_circular_buffer_empty","[test_circular_buffer]",
    (experimental_multithreading::spsc_circular_buffer<float, 10>),
    (experimental_multithreading::mpmc_circular_buffer<float, 10>),
    (experimental_multithreading::mpmc_lock_free_circular_buffer_v1<float, 10>),
    (experimental_multithreading::mpmc_lock_free_circular_buffer_v2<float, 10>)
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
    (experimental_multithreading::mpmc_lock_free_circular_buffer_v2<float, 10>)
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
    (experimental_multithreading::mpmc_lock_free_circular_buffer_v2<float, 10>)
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

TEST_CASE("test_mpmc_circular_buffer_multithread","[test_circular_buffer]"){
    using value_type = float;
    using test_circular_buffer::make_ranges;
    static constexpr std::size_t n_producers = 10;
    static constexpr std::size_t n_consumers = 10;
    static constexpr std::size_t buffer_size = 100;
    //static constexpr std::size_t n_elements = 1000;
    static constexpr std::size_t n_elements = 1024*1024;
    //static constexpr std::size_t n_elements = 128*1024;
    //using buffer_type = experimental_multithreading::mpmc_circular_buffer<value_type, buffer_size>;
    //using buffer_type = experimental_multithreading::mpmc_lock_free_circular_buffer_v1<value_type, buffer_size>;
    //using buffer_type = experimental_multithreading::mpmc_lock_free_circular_buffer_v1<value_type, buffer_size, unsigned char>;
    using buffer_type = experimental_multithreading::mpmc_lock_free_circular_buffer_v2<value_type, buffer_size, unsigned char>;
    //using buffer_type = experimental_multithreading::mpmc_lock_free_circular_buffer_v2<value_type, buffer_size>;
    //using buffer_type = experimental_multithreading::spsc_circular_buffer<value_type, buffer_size>;

    buffer_type buffer{};
    std::vector<value_type> expected(n_elements);
    for (std::size_t i{0}; i!=n_elements; ++i){
        expected[i] = static_cast<value_type>(i);
    }
    std::atomic<std::size_t> producer_counter{0};
    auto producer_f = [&buffer,&producer_counter](auto first, auto last){
        std::for_each(first,last,
            [&buffer,&producer_counter](const auto& v){
                while(!buffer.try_push(v)){
                    //std::cout<<std::endl<<"producer_f";
                }
                producer_counter.fetch_add(1);
            }
        );
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
    for(auto it = producer_ranges.begin(); it!=producer_ranges.end()-1; ++it,++producers_it){
        *producers_it = std::thread(producer_f, expected.begin()+*it , expected.begin()+*(it+1));
    }
    std::vector<value_type> result(n_elements);
    static constexpr auto consumer_ranges = make_ranges<n_elements,n_consumers>();
    auto consumers_it = consumers.begin();
    for(auto it = consumer_ranges.begin(); it!=consumer_ranges.end()-1; ++it,++consumers_it){
        *consumers_it = std::thread(consumer_f, result.begin()+*it , result.begin()+*(it+1));
    }


    // std::this_thread::sleep_for(std::chrono::milliseconds{5000});
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


    std::cout<<std::endl<<"join_producers...";
    std::for_each(producers.begin(),producers.end(),[](auto& t){t.join();});
    std::cout<<std::endl<<"producers_counter"<<producer_counter.load();


    std::cout<<std::endl<<"join...consumers";
    std::for_each(consumers.begin(),consumers.end(),[](auto& t){t.join();});
    std::cout<<std::endl<<"consumers_counter"<<consumer_counter.load();

    std::set<value_type> expected_(expected.begin(),expected.end());
    std::set<value_type> result_(result.begin(),result.end());
    REQUIRE(result_ == expected_);
    REQUIRE(buffer.size() == 0);
}

