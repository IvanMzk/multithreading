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
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    counter.fetch_add(1);
}
void g(){
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    counter.fetch_sub(1);
}

template<typename It>
auto accumulate(It first, It last){
    return std::accumulate(first, last,  static_cast<std::iterator_traits<It>::value_type>(0));
}

}   //end of namespace test_thread_pool


TEST_CASE("test_thread_pool_no_result" , "[test_thread_pool]"){
    using test_thread_pool::counter;
    using test_thread_pool::f;
    using test_thread_pool::g;
    using experimental_multithreading::thread_pool;
    using experimental_multithreading::task_future;

    constexpr static std::size_t n_threads = 10;
    constexpr static std::size_t n_tasks = 1000;
    thread_pool<n_threads,void(void)> pool;
    std::array<task_future<void>, n_tasks> futures;
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

TEST_CASE("test_thread_pool_result" , "[test_thread_pool]"){
    using test_thread_pool::accumulate;
    using value_type = std::size_t;
    using container_type = std::vector<value_type>;
    using iterator_type = typename container_type::iterator;
    using experimental_multithreading::thread_pool;
    using experimental_multithreading::task_future;

    constexpr static std::size_t n_elements = 10*1000*1000;
    constexpr static std::size_t n_threads = 4;
    thread_pool<n_threads,value_type(iterator_type,iterator_type)> pool;

    std::array<task_future<value_type>, n_threads> futures;
    container_type elements(n_elements,static_cast<value_type>(1));
    static constexpr auto elements_ranges = benchmark_helpers::make_ranges<n_elements,n_threads>();
    std::size_t i{0};
    //benchmark_helpers::cpu_timer start{};
    for(auto it = elements_ranges.begin(); it!=elements_ranges.end()-1; ++it,++i){
        futures[i] = pool.push(static_cast<value_type(*)(iterator_type,iterator_type)>(accumulate), elements.begin()+*it , elements.begin()+*(it+1));
    }
    value_type sum{0};
    std::for_each(futures.begin(), futures.end(), [&sum](auto& f){sum+=f.get();});
    //benchmark_helpers::cpu_timer stop{};
    //std::cout<<std::endl<<"n_threads"<<n_threads<<" ms "<<stop-start;
    REQUIRE(sum == n_elements);

}