#ifndef THREAD_POOL_HPP_
#define THREAD_POOL_HPP_
#include <iostream>
#include <array>
#include <tuple>
#include <thread>
#include <mutex>
#include <future>
#include <condition_variable>
#include "mpmc_bounded_queue.hpp"

namespace experimental_multithreading{

template<typename R>
class task_future{
    using result_type = R;
    std::future<result_type> f;
public:
    ~task_future(){
        if (f.valid()){
            f.wait();
        }
    }
    task_future() = default;
    task_future(task_future&&) = default;
    task_future& operator=(task_future&&) = default;
    task_future(std::future<result_type>&& f_):
        f{std::move(f_)}
    {}
    operator bool(){return f.valid();}
    void wait()const{f.wait();}
    auto get(){return f.get();}
};


template<typename R, typename...Args>
class task
{
    using func_ptr_type = R(*)(Args...);
    using result_type = R;
    using future_type = task_future<result_type>;

    func_ptr_type f;
    std::tuple<Args...> args;
    std::promise<result_type> task_promise;

public:
    template<typename...Args_>
    task(func_ptr_type f_, Args_&&...args_):
        f{f_},
        args{std::forward<Args_>(args_)...}
    {}
    auto call(){
        if constexpr(std::is_void_v<result_type>){
            std::apply(f, std::move(args));
            task_promise.set_value();
        }else{
            task_promise.set_value(std::apply(f, std::move(args)));
        }
    }
    auto get_future(){
        return future_type{task_promise.get_future()};
    }
};


template<std::size_t N, typename> class thread_pool;

//specialization for function pointer
template<std::size_t N, typename R, typename...Args>
class thread_pool<N, R(Args...)>
{
    static constexpr std::size_t n_workers = N;
    using func_ptr_type = R(*)(Args...);
    using task_type = task<R,Args...>;
    using queue_type = mpmc_bounded_queue::st_bounded_queue<task_type, n_workers>;
    using mutex_type = std::mutex;

public:

    ~thread_pool()
    {
        stop();
    }
    thread_pool()
    {
        init();
    }

    template<typename...Args_>
    auto try_push(func_ptr_type f, Args_&&...args){
        std::unique_lock<mutex_type> lock{guard};
        if (auto t = tasks.try_push(f,std::forward<Args_>(args)...)){
            auto valid_task_future = t->get_future();
            lock.unlock();
            has_task.notify_one();
            return valid_task_future;
        }else{
            lock.unlock();
            return task_future<R>{};   //empty task_future
        }
    }

    template<typename...Args_>
    auto push(func_ptr_type f, Args_&&...args){
        while(true){
            if (auto res = try_push(f, std::forward<Args_>(args)...)){
                return res;
            }
            std::this_thread::yield();
        }
    }

private:

    void init(){
        for (std::size_t i{0}; i!=n_workers; ++i){
            workers[i] = std::thread{&thread_pool::worker_loop, this};
        }
    }

    void stop(){
        finish_workers.store(true);
        has_task.notify_all();
        for (std::size_t i{0}; i!=n_workers; ++i){
            workers[i].join();
        }
    }

    //problem is to use waiting not yealding in loop and have concurrent push and pop
    //conditional_variable must use same mutex to guard push and pop, even if queue is mpmc
    void worker_loop(){
        while(!finish_workers.load()){
            std::unique_lock<mutex_type> lock{guard};
            while(!finish_workers.load()){
                if (auto t = tasks.try_pop()){
                    lock.unlock();
                    t.get().call();
                    break;
                }else{
                    has_task.wait(lock);
                }
            }
        }
    }

    std::atomic<bool> finish_workers{false};
    mutex_type guard;
    std::condition_variable has_task;
    queue_type tasks;
    std::array<std::thread, n_workers> workers;
};

}   //end of namespace experimental_multithreading

#endif