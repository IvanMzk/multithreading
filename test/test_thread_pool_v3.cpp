#include <string>
#include <array>
#include <atomic>
#include <numeric>
#include <vector>
#include "catch.hpp"
#include "thread_pool.hpp"
#include "benchmark_helpers.hpp"

namespace test_thread_pool_v3{

static std::atomic<std::size_t> counter{0};
void f(){
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    counter.fetch_add(1);
}
void g(std::size_t x){
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    counter.fetch_add(x);
}
struct h{
    template<typename T>
    void operator()(T x){
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        counter.fetch_sub(x);
    }
};
struct q{
    void operator()(){
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        counter.fetch_sub(1);
    }
};
struct v{
    void f(std::size_t x){
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        counter.fetch_add(x);
    }
};

}   //end of namespace test_thread_pool_v3


TEMPLATE_TEST_CASE("test_thread_pool_v3_v4_void_result","[test_thread_pool_v3_v4]",
    thread_pool::thread_pool_v3,
    thread_pool::thread_pool_v4
)
{
    using thread_pool::task_future;
    using tread_pool_type = TestType;
    using test_thread_pool_v3::f;
    using test_thread_pool_v3::g;
    using test_thread_pool_v3::h;
    using test_thread_pool_v3::q;
    using test_thread_pool_v3::v;
    using test_thread_pool_v3::counter;
    using benchmark_helpers::cpu_timer;

    constexpr static std::size_t n_threads = 100;
    constexpr static std::size_t n_tasks = 1*100*1000;
    tread_pool_type pool{n_threads};
    std::vector<task_future<void>> futures(n_tasks);
    counter.store(0);
    std::size_t counter_{0};
    v v_{};
    for (std::size_t i{0}; i!=n_tasks; ++i){
        switch(i%5)
        {
            case 0:
                if(i%2){futures[i] = pool.push(f);}else{futures[i] = pool.push_async(f);}   //add 1
                ++counter_;
                break;
            case 1:
                if(i%2){futures[i] = pool.push(g,i);}else{futures[i] = pool.push_async(g,i);}  //add i
                counter_+=i;
                break;
            case 2:
                if(i%2){futures[i] = pool.push(h{},i);}else{futures[i] = pool.push_async(h{},i);}  //sub i
                counter_-=i;
                break;
            case 3:
                if(i%2){futures[i] = pool.push(q{});}else{futures[i] = pool.push_async(q{});}  //sub 1
                --counter_;
                break;
            case 4:
                if(i%2){futures[i] = pool.push(&v::f, &v_, i);}else{futures[i] = pool.push_async(&v::f, &v_, i);}   //add i
                counter_+=i;
                break;
            default :
                break;
        }
    }
    std::for_each(futures.begin(), futures.end(), [](auto& f){f.wait();});
    REQUIRE(counter == counter_);
}

TEMPLATE_TEST_CASE("test_thread_pool_v3_v4_result","[test_thread_pool_v3_v4]",
    thread_pool::thread_pool_v3,
    thread_pool::thread_pool_v4
)
{
    using thread_pool::task_future;
    using thread_pool_type = TestType;
    using value_type = std::size_t;
    using benchmark_helpers::cpu_timer;

    constexpr static std::size_t n_elements = 1*1000*1000;
    constexpr static std::size_t n_threads = 10;
    thread_pool_type pool{n_threads};

    std::array<task_future<value_type>, n_threads> futures;
    std::vector<value_type> elements(n_elements);
    std::iota(elements.begin(),elements.end(),value_type{1});   //1,2,3,...,n_elements
    static constexpr auto elements_ranges = benchmark_helpers::make_ranges<n_elements,n_threads>();
    std::size_t i{0};
    for(auto it = elements_ranges.begin(); it!=elements_ranges.end()-1; ++it,++i){
        if (i%2){
            futures[i] = pool.push([](auto first, auto last){return std::accumulate(first,last,typename std::iterator_traits<decltype(first)>::value_type{0});}, elements.begin()+*it , elements.begin()+*(it+1));
        }else{
            futures[i] = pool.push_async([](auto first, auto last){return std::accumulate(first,last,typename std::iterator_traits<decltype(first)>::value_type{0});}, elements.begin()+*it , elements.begin()+*(it+1));
        }
    }
    value_type result_sum{0};
    std::for_each(futures.begin(), futures.end(), [&result_sum](auto& f){result_sum+=f.get();});
    value_type expected_sum = (n_elements*(n_elements+1))/2;
    REQUIRE(result_sum == expected_sum);
}

TEMPLATE_TEST_CASE("test_thread_pool_v3_task_group","[test_thread_pool_v3]",
    thread_pool::thread_pool_v3,
    thread_pool::thread_pool_v4
)
{
    using thread_pool::task_group;
    using tread_pool_type = TestType;

    constexpr static std::size_t n_threads = 10;
    tread_pool_type pool{n_threads};
    task_group group{};
    std::atomic<std::size_t> counter{0};

    auto inc = [](auto& i, auto delay_ms){
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        i.fetch_add(1);
    };

    pool.push_group(group,inc,std::ref(counter),10);
    pool.push_group(group,inc,std::ref(counter),10);
    pool.push_group(group,inc,std::ref(counter),100);
    pool.push_group(group,inc,std::ref(counter),100);
    pool.push_group(group,inc,std::ref(counter),1000);
    pool.push_group(group,inc,std::ref(counter),1000);
    pool.push_group(group,inc,std::ref(counter),1000);
    pool.push_group(group,inc,std::ref(counter),1000);
    pool.push_group(group,inc,std::ref(counter),1000);
    pool.push_group(group,inc,std::ref(counter),1000);
    group.wait();
    REQUIRE(counter.load() == 10);
}

TEMPLATE_TEST_CASE("test_thread_pool_v3_task_group_many_tasks","[test_thread_pool_v3]",
    thread_pool::thread_pool_v3,
    thread_pool::thread_pool_v4
)
{
    using thread_pool::task_group;
    using tread_pool_type = TestType;
    using test_thread_pool_v3::f;
    using test_thread_pool_v3::g;
    using test_thread_pool_v3::h;
    using test_thread_pool_v3::q;
    using test_thread_pool_v3::v;
    using test_thread_pool_v3::counter;

    constexpr static std::size_t n_threads = 100;
    constexpr static std::size_t n_tasks = 1*100*1000;
    tread_pool_type pool{n_threads};
    task_group group{};
    counter.store(0);
    std::size_t counter_{0};
    v v_{};
    for (std::size_t i{0}; i!=n_tasks; ++i){
        switch(i%5)
        {
            case 0:
                pool.push_group(group,f);
                ++counter_;
                break;
            case 1:
                pool.push_group(group,g,i);
                counter_+=i;
                break;
            case 2:
                pool.push_group(group,h{},i);
                counter_-=i;
                break;
            case 3:
                pool.push_group(group,q{});
                --counter_;
                break;
            case 4:
                pool.push_group(group,&v::f, &v_, i);
                counter_+=i;
                break;
            default :
                break;
        }
    }
    group.wait();
    REQUIRE(counter == counter_);
}