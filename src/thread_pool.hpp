#ifndef THREAD_POOL_HPP_
#define THREAD_POOL_HPP_
#include <array>
#include <tuple>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "mpmc_bounded_queue.hpp"



template<typename R, typename...Args>
class task
{
    using func_ptr_type = R(*)(Args&&...);
    func_ptr_type f;
    std::tuple<Args...> args;
public:
    template<typename...Args_>
    task(func_ptr_type f_, Args_&&...args_):
        f{f_},
        args{std::forward<Args_>(args_)...}
    {}
    auto call()const{
        return std::apply(f, args);
    }
};


template<std::size_t N, typename> class thread_pool;

//specialization for functor of specific type
//R, Args... extracts from functor's operator() signature

//specialization for std::function<R(Args...)>

//specialization for function pointer
template<std::size_t N, typename R, typename...Args>
class thread_pool<N, R(Args...)>
{
    static constexpr std::size_t n_workers = N;
    using task_type = task<R,Args...>;
    using queue_type = mpmc_bounded_queue::st_bounded_queue<task_type, n_workers>;
    using mutex_type = std::mutex;
    //using queue_type = mpmc_bounded_queue::mpmc_bounded_queue_v1<task_type, n_workers>;

public:

    thread_pool():
    {
        init();
    }
    ~thread_pool()
    {

    }


    template<typename F, typename...Args_>
    bool try_push( F f, Args_&&...args){
        std::unique_lock<mutex_type> lock{guard};
        if (tasks.try_push(f,std::forward<Args_>(args)...)){
            lock.unlock();
            has_task.notify_one();
            return true;
        }else{
            return false;
        }
    }


private:

    void init(){
        for (std::size_t i{0}; i!=n_workers; ++i){
            workers[i] = std::thread{worker_loop};
        }
    }

    //problem is to use waiting not yealding in loop and have concurrent push and pop
    //conditional_variable must use same mutex to gurd push and pop, even if queue is mpmc
    void worker_loop(){
        while(true){
            std::unique_lock<mutex_type> lock{guard};
            has_task.wait(lock, [this]{return })


        }
    }

    mutex_type guard;
    std::condition_variable has_task;
    queue_type tasks;
    std::array<std::thread, n_workers> workers;

};

#endif