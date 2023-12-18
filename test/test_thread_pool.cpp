#include <string>
#include <array>
#include <atomic>
#include <numeric>
#include <vector>
#include "catch.hpp"
#include "thread_pool.hpp"
#include "benchmark_helpers.hpp"

namespace test_thread_pool{

static std::atomic<std::size_t> counter{0};
void f(){
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    counter.fetch_add(1);
}
void g(){
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    counter.fetch_sub(1);
}
}   //end of namespace test_thread_pool
TEMPLATE_TEST_CASE("test_thread_pool_no_result" , "[test_thread_pool]",
    thread_pool::thread_pool_v1<void(void)>,
    thread_pool::thread_pool_v2<void(void)>
)
{
    using test_thread_pool::counter;
    using test_thread_pool::f;
    using test_thread_pool::g;
    using thread_pool::task_future;
    using tread_pool_type = TestType;

    constexpr static std::size_t n_threads = 100;
    constexpr static std::size_t queue_capacity = n_threads;
    constexpr static std::size_t n_tasks = 1*100*1000;
    tread_pool_type pool{n_threads, queue_capacity};
    std::vector<task_future<void>> futures(n_tasks);
    counter.store(0);
    for (std::size_t i{0}; i!=n_tasks; ++i){
        if(i%2){
            futures[i] = pool.push(g);
        }else{
            futures[i] = pool.push(f);
        }
    }
    std::for_each(futures.begin(), futures.end(), [](auto& f){f.wait();});
    REQUIRE(counter == 0+n_tasks%2);
}

namespace test_thread_pool_result{
using value_type = std::size_t;
using container_type = std::vector<value_type>;
using iterator_type = typename container_type::iterator;
template<typename It>
auto accumulate(It first, It last){
    return std::accumulate(first, last,  static_cast<typename std::iterator_traits<It>::value_type>(0));
}
}   //end of namespace test_thread_pool_result
TEMPLATE_TEST_CASE("test_thread_pool_result" , "[test_thread_pool]",
    thread_pool::thread_pool_v1<test_thread_pool_result::value_type(test_thread_pool_result::iterator_type,test_thread_pool_result::iterator_type)>,
    thread_pool::thread_pool_v2<test_thread_pool_result::value_type(test_thread_pool_result::iterator_type,test_thread_pool_result::iterator_type)>
)
{
    using test_thread_pool_result::accumulate;
    using thread_pool::task_future;
    using thread_pool_type = TestType;

    using value_type = test_thread_pool_result::value_type;
    using container_type = test_thread_pool_result::container_type;
    using iterator_type = test_thread_pool_result::iterator_type;

    constexpr static std::size_t n_elements = 1*1000*1000;
    constexpr static std::size_t n_threads = 4;
    constexpr static std::size_t queue_capacity = 10;
    thread_pool_type pool{n_threads,queue_capacity};

    std::array<task_future<value_type>, n_threads> futures;
    container_type elements(n_elements,static_cast<value_type>(1));
    static constexpr auto elements_ranges = benchmark_helpers::make_ranges<n_elements,n_threads>();
    std::size_t i{0};
    for(auto it = elements_ranges.begin(); it!=elements_ranges.end()-1; ++it,++i){
        futures[i] = pool.push(static_cast<value_type(*)(iterator_type,iterator_type)>(accumulate), elements.begin()+*it , elements.begin()+*(it+1));
    }
    value_type sum{0};
    std::for_each(futures.begin(), futures.end(), [&sum](auto& f){sum+=f.get();});
    REQUIRE(sum == n_elements);
}

