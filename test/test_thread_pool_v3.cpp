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
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    counter.fetch_add(1);
}
void g(std::size_t x){
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    counter.fetch_add(x);
}
struct h{
    void operator()(std::size_t x){
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        counter.fetch_sub(x);
    }
};
struct q{
    void operator()(){
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        counter.fetch_sub(1);
    }
};
struct v{
    void f(std::size_t x){
        counter.fetch_add(x);
    }
};

}   //end of namespace test_thread_pool_v3

TEST_CASE("test_thread_pool_v3_void_result","[test_thread_pool_v3]"){
    using thread_pool::task_future;
    using tread_pool_type = thread_pool::thread_pool_v3;
    using test_thread_pool_v3::f;
    using test_thread_pool_v3::g;
    using test_thread_pool_v3::h;
    using test_thread_pool_v3::q;
    using test_thread_pool_v3::v;
    using test_thread_pool_v3::counter;

    constexpr static std::size_t n_threads = 1000;
    constexpr static std::size_t queue_capacity = n_threads;
    constexpr static std::size_t n_tasks = 1000*1000;
    tread_pool_type pool{n_threads, queue_capacity};
    std::array<task_future<void>, n_tasks> futures;
    counter.store(0);
    std::size_t counter_{0};
    v v_{};
    for (std::size_t i{0}; i!=n_tasks; ++i){
        switch(i%5)
        {
            case 0:
                futures[i] = pool.push(f);    //add 1
                ++counter_;
                break;
            case 1:
                futures[i] = pool.push(g,i);  //add i
                counter_+=i;
                break;
            case 2:
                futures[i] = pool.push(h{},i);  //sub i
                counter_-=i;
                break;
            case 3:
                futures[i] = pool.push(q{});  //sub 1
                --counter_;
                break;
            case 4:
                futures[i] = pool.push(&v::f, &v_, i);
                counter_+=i;
                break;
            default :
                break;
        }
    }
    std::for_each(futures.begin(), futures.end(), [](auto& f){f.wait();});
    REQUIRE(counter == counter_);
}