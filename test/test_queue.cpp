
#include <thread>
#include <vector>
#include <set>
#include <iostream>
#include "catch.hpp"
#include "benchmark_helpers.hpp"
#include "queue.hpp"

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
TEMPLATE_TEST_CASE("test_mpmc_bounded_queue_non_blocking_interface","[test_mpmc_bounded_queue]",
    (queue::mpmc_bounded_queue_v1<test_mpmc_bounded_queue_single_thread::value_type>),
    (queue::mpmc_bounded_queue_v2<test_mpmc_bounded_queue_single_thread::value_type>),
    (queue::mpmc_bounded_queue_v3<test_mpmc_bounded_queue_single_thread::value_type>),
    (queue::st_bounded_queue<test_mpmc_bounded_queue_single_thread::value_type>)
){
    using queue_type = TestType;
    using value_type = typename queue_type::value_type;

    queue_type queue{test_mpmc_bounded_queue_single_thread::capacity};
    REQUIRE(queue.size() == 0);

    const value_type v_{11};
    const value_type v_to_push{v_+1};
    value_type v{v_};
    SECTION("empty_queue"){
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
    SECTION("full_queue"){
        const std::size_t capacity = queue.capacity();
        std::size_t i{0};
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
}

TEMPLATE_TEST_CASE("test_mpmc_bounded_queue_blocking_interface","[test_mpmc_bounded_queue]",
    (queue::mpmc_bounded_queue_v1<test_mpmc_bounded_queue_single_thread::value_type>),
    (queue::mpmc_bounded_queue_v2<test_mpmc_bounded_queue_single_thread::value_type>),
    (queue::mpmc_bounded_queue_v3<test_mpmc_bounded_queue_single_thread::value_type>)
){
    using queue_type = TestType;
    using value_type = typename queue_type::value_type;

    queue_type queue{test_mpmc_bounded_queue_single_thread::capacity};
    REQUIRE(queue.size() == 0);

    const value_type v_{11};
    const value_type v_to_push{v_+1};
    value_type v{v_};
    SECTION("empty_queue"){
        queue.push(v_to_push);
        REQUIRE(queue.size() == 1);
        queue.pop(v);
        REQUIRE(!queue.try_pop(v));
        REQUIRE(queue.size() == 0);
        REQUIRE(v == v_to_push);
    }
    SECTION("full_queue"){
        const std::size_t capacity = queue.capacity();
        std::size_t i{0};
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
    (queue::mpmc_bounded_queue_v1<test_mpmc_bounded_queue_single_thread::constructor_destructor_counter>),
    (queue::mpmc_bounded_queue_v2<test_mpmc_bounded_queue_single_thread::constructor_destructor_counter>),
    (queue::mpmc_bounded_queue_v3<test_mpmc_bounded_queue_single_thread::constructor_destructor_counter>),
    (queue::st_bounded_queue<test_mpmc_bounded_queue_single_thread::constructor_destructor_counter>)
){
    using queue_type = TestType;
    using value_type = typename queue_type::value_type;
    const std::size_t capacity = test_mpmc_bounded_queue_single_thread::capacity;

    value_type::reset_constructor_counter();
    value_type::reset_destructor_counter();

    SECTION("full_queue_1"){
        {
            queue_type queue{test_mpmc_bounded_queue_single_thread::capacity};
            while(queue.try_push());
            REQUIRE(queue.size() == capacity);
        }
        REQUIRE (value_type::destructor_counter() == capacity);
    }
    SECTION("full_queue_2"){
        {
            queue_type queue{test_mpmc_bounded_queue_single_thread::capacity};
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
            queue_type queue{test_mpmc_bounded_queue_single_thread::capacity};
        }
        REQUIRE (value_type::destructor_counter() == 0);
        REQUIRE (value_type::constructor_counter() == 0);
    }
    SECTION("empty_queue_2"){
        {
            queue_type queue{test_mpmc_bounded_queue_single_thread::capacity};
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
    static constexpr std::size_t n_elements = 1*1000*1000;
    static constexpr std::size_t capacity = 32;

    struct producer{
        template<typename U, typename V>
        void operator()(std::reference_wrapper<U> queue, V first, V last){
            auto try_push = [&queue](const auto& v){while(!queue.get().try_push(v));};
            auto push = [&queue](const auto& v){queue.get().push(v);};
            std::size_t i{0};
            std::for_each(first,last,[&](const auto& v){
                switch (i%2){
                    case 0: try_push(v); break;
                    case 1: push(v); break;
                }
                ++i;
            });
        }
    };
    struct consumer{
        template<typename U, typename V>
        void operator()(std::reference_wrapper<U> queue, V first, V last){
            auto try_pop = [&queue](auto& v){while(!queue.get().try_pop(v));};
            auto try_pop_element = [&queue](auto& v){
                while(true){
                    if (auto v_ = queue.get().try_pop()){
                        v = v_.get();
                        break;
                    }
                }
            };
            auto pop = [&queue](auto& v){queue.get().pop(v);};
            auto pop_element = [&queue](auto& v){v = queue.get().pop().get();};
            std::size_t i{0};
            std::for_each(first,last,[&](auto& v){
                switch (i%4){
                    case 0: try_pop(v); break;
                    case 1: try_pop_element(v); break;
                    case 2: pop(v); break;
                    case 3: pop_element(v); break;
                }
                ++i;
            });
        }

    };
}
TEMPLATE_TEST_CASE("test_mpmc_bounded_queue_multithread","[test_mpmc_bounded_queue]",
    (queue::mpmc_bounded_queue_v1<test_mpmc_bounded_queue_multithread::value_type>),
    (queue::mpmc_bounded_queue_v2<test_mpmc_bounded_queue_multithread::value_type>),
    (queue::mpmc_bounded_queue_v3<test_mpmc_bounded_queue_multithread::value_type>)
)
{
    using benchmark_helpers::make_ranges;
    using queue_type = TestType;
    using producer_type = test_mpmc_bounded_queue_multithread::producer;
    using consumer_type = test_mpmc_bounded_queue_multithread::consumer;
    using value_type = typename queue_type::value_type;
    static constexpr std::size_t n_elements = test_mpmc_bounded_queue_multithread::n_elements;
    static constexpr std::size_t n_producers = 10;
    static constexpr std::size_t n_consumers = 10;
    queue_type queue{test_mpmc_bounded_queue_multithread::capacity};

    std::vector<value_type> expected(n_elements);
    for (std::size_t i{0}; i!=n_elements; ++i){
        expected[i] = static_cast<value_type>(i);
    }

    std::array<std::thread, n_producers> producers;
    std::array<std::thread, n_consumers> consumers;

    static constexpr auto producer_ranges = make_ranges<n_elements,n_producers>();
    auto producers_it = producers.begin();
    for(auto it = producer_ranges.begin(); it!=producer_ranges.end()-1; ++it,++producers_it){
        *producers_it = std::thread(producer_type{}, std::reference_wrapper<queue_type>{queue}, expected.begin()+*it , expected.begin()+*(it+1));
    }
    std::vector<value_type> result(n_elements);
    static constexpr auto consumer_ranges = make_ranges<n_elements,n_consumers>();
    auto consumers_it = consumers.begin();
    for(auto it = consumer_ranges.begin(); it!=consumer_ranges.end()-1; ++it,++consumers_it){
        *consumers_it = std::thread(consumer_type{}, std::reference_wrapper<queue_type>{queue}, result.begin()+*it , result.begin()+*(it+1));
    }

    std::for_each(producers.begin(),producers.end(),[](auto& t){t.join();});
    std::for_each(consumers.begin(),consumers.end(),[](auto& t){t.join();});

    std::sort(result.begin(),result.end());
    REQUIRE(result.size() == expected.size());
    REQUIRE(result == expected);
    REQUIRE(queue.size() == 0);
}

namespace test_st_queue_of_polymorphic{

inline constexpr std::size_t neg_alignment = 1024;

template<typename T>
class ctr_dtr_counter
{
    static std::size_t ctr_counter_;
    static std::size_t dtr_counter_;
public:
    ~ctr_dtr_counter(){++dtr_counter_;}
    ctr_dtr_counter(){++ctr_counter_;}
    static auto ctr_counter(){return ctr_counter_;}
    static auto dtr_counter(){return dtr_counter_;}
    static auto reset_counters(){
        ctr_counter_ = 0;
        dtr_counter_ = 0;
    }
};
template<typename T> std::size_t ctr_dtr_counter<T>::ctr_counter_{0};
template<typename T> std::size_t ctr_dtr_counter<T>::dtr_counter_{0};

template<typename T>
class operation
{
public:
    virtual ~operation(){}
    virtual T call() = 0;
};

template<typename T>
class alignas(neg_alignment) neg  :
    private ctr_dtr_counter<neg<T>>,
    public operation<T>
{
    T op;
public:
    ~neg(){}
    T call()override{return -op;}
    neg(const T& op_):
        op{op_}
    {}
};

template<typename T>
class binary_add  :
    private ctr_dtr_counter<binary_add<T>>,
    public operation<T>
{
    T op1;
    T op2;
public:
    T call()override{return op1+op2;}
    binary_add(const T& op1_, const T& op2_):
        op1{op1_},
        op2{op2_}
    {}
};

template<typename T>
class binary_mul  :
    private ctr_dtr_counter<binary_mul<T>>,
    public operation<T>
{
    T op1;
    T op2;
public:
    T call()override{return op1*op2;}
    binary_mul(const T& op1_, const T& op2_):
        op1{op1_},
        op2{op2_}
    {}
};

}   //end of namespace test_st_queue_of_polymorphic

TEST_CASE("test_st_queue_of_polymorphic", "[test_st_queue_of_polymorphic]"){
    using queue::st_queue_of_polymorphic;
    using test_st_queue_of_polymorphic::operation;
    using test_st_queue_of_polymorphic::neg;
    using test_st_queue_of_polymorphic::binary_add;
    using test_st_queue_of_polymorphic::binary_mul;
    using test_st_queue_of_polymorphic::neg_alignment;
    using test_st_queue_of_polymorphic::ctr_dtr_counter;
    using value_type = int;

    ctr_dtr_counter<neg<value_type>>::reset_counters();
    ctr_dtr_counter<binary_add<value_type>>::reset_counters();
    ctr_dtr_counter<binary_mul<value_type>>::reset_counters();

    st_queue_of_polymorphic<operation<value_type>> queue{};
    REQUIRE(queue.size() == 0);
    REQUIRE(!queue.try_pop());

    SECTION("test_single_element"){
        auto neg_impl = queue.push<neg<value_type>>(1);
        REQUIRE(queue.size() == 1);
        REQUIRE(neg_impl);
        REQUIRE(reinterpret_cast<std::uintptr_t>(neg_impl)%neg_alignment == 0);
        REQUIRE(ctr_dtr_counter<neg<value_type>>::ctr_counter() == 1);
        REQUIRE(ctr_dtr_counter<neg<value_type>>::dtr_counter() == 0);
        REQUIRE(neg_impl->call() == -1);
        {
            auto neg_op = queue.try_pop();
            REQUIRE(neg_op);
            REQUIRE(neg_op->call() == -1);
            REQUIRE(queue.size() == 0);
            REQUIRE(!queue.try_pop());
        }
        REQUIRE(ctr_dtr_counter<neg<value_type>>::ctr_counter() == 1);
        REQUIRE(ctr_dtr_counter<neg<value_type>>::dtr_counter() == 1);
    }
    SECTION("test_many_elements"){
        static constexpr std::size_t n_iters = 1000;
        static constexpr std::size_t n_elements = n_iters*3;

        std::vector<value_type> expected{};
        expected.reserve(n_elements);
        for (std::size_t i{0}; i!=n_iters; ++i){
            auto neg_impl = queue.push<neg<value_type>>(static_cast<value_type>(i));
            expected.push_back(neg_impl->call());
            auto add_impl = queue.push<binary_add<value_type>>(static_cast<value_type>(i),2);
            expected.push_back(add_impl->call());
            auto mul_impl = queue.push<binary_mul<value_type>>(2,static_cast<value_type>(i));
            expected.push_back(mul_impl->call());
        }
        REQUIRE(queue.size() == n_elements);
        REQUIRE(ctr_dtr_counter<neg<value_type>>::ctr_counter() == n_iters);
        REQUIRE(ctr_dtr_counter<binary_add<value_type>>::ctr_counter() == n_iters);
        REQUIRE(ctr_dtr_counter<binary_mul<value_type>>::ctr_counter() == n_iters);
        REQUIRE(ctr_dtr_counter<neg<value_type>>::dtr_counter() == 0);
        REQUIRE(ctr_dtr_counter<binary_add<value_type>>::dtr_counter() == 0);
        REQUIRE(ctr_dtr_counter<binary_mul<value_type>>::dtr_counter() == 0);

        std::vector<value_type> result{};
        result.reserve(n_elements);
        while(auto elem = queue.try_pop()){
            result.push_back(elem->call());
        }
        REQUIRE(queue.size() == 0);
        REQUIRE(ctr_dtr_counter<neg<value_type>>::ctr_counter() == n_iters);
        REQUIRE(ctr_dtr_counter<binary_add<value_type>>::ctr_counter() == n_iters);
        REQUIRE(ctr_dtr_counter<binary_mul<value_type>>::ctr_counter() == n_iters);
        REQUIRE(ctr_dtr_counter<neg<value_type>>::dtr_counter() == n_iters);
        REQUIRE(ctr_dtr_counter<binary_add<value_type>>::dtr_counter() == n_iters);
        REQUIRE(ctr_dtr_counter<binary_mul<value_type>>::dtr_counter() == n_iters);

        REQUIRE(std::equal(result.begin(), result.end(), expected.begin()));
    }
    SECTION("test_clear_queue_on_destruction"){
        {
            st_queue_of_polymorphic<operation<value_type>> queue_{};
            queue_.push<neg<value_type>>(1);
            queue_.push<binary_add<value_type>>(1,2);
            queue_.push<binary_mul<value_type>>(2,1);
            queue_.push<neg<value_type>>(1);
            queue_.push<binary_mul<value_type>>(2,1);
            queue_.try_pop();
            queue_.try_pop();
            REQUIRE(ctr_dtr_counter<neg<value_type>>::dtr_counter() == 1);
            REQUIRE(ctr_dtr_counter<binary_add<value_type>>::dtr_counter() == 1);
            REQUIRE(ctr_dtr_counter<binary_mul<value_type>>::dtr_counter() == 0);
        }
        REQUIRE(ctr_dtr_counter<neg<value_type>>::ctr_counter() == 2);
        REQUIRE(ctr_dtr_counter<binary_add<value_type>>::ctr_counter() == 1);
        REQUIRE(ctr_dtr_counter<binary_mul<value_type>>::ctr_counter() == 2);
    }
}
