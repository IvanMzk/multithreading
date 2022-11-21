
#include <thread>
#include <vector>
#include "catch.hpp"
#include "circular_buffer.hpp"


TEST_CASE("test_circular_buffer_empty","[test_circular_buffer]"){
    using value_type = float;
    using experimental_multithreading::spsc_circular_buffer;
    static constexpr std::size_t buffer_size = 10;

    spsc_circular_buffer<value_type, buffer_size> buffer{};

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

TEST_CASE("test_circular_buffer_full","[test_circular_buffer]"){
    using value_type = float;
    using experimental_multithreading::spsc_circular_buffer;
    static constexpr std::size_t buffer_size = 10;

    spsc_circular_buffer<value_type, buffer_size> buffer{};

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

TEST_CASE("test_circular_buffer_overwrite","[test_circular_buffer]"){
    using value_type = float;
    using experimental_multithreading::spsc_circular_buffer;
    static constexpr std::size_t buffer_size = 10;

    spsc_circular_buffer<value_type, buffer_size> buffer{};

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

TEST_CASE("test_circular_buffer_multithread","[test_circular_buffer]"){
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

