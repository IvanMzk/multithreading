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
    bool sync_;
    std::future<result_type> f;
public:
    ~task_future(){
        if (sync_ && f.valid()){
            f.wait();
        }
    }
    task_future() = default;
    task_future(task_future&&) = default;
    task_future& operator=(task_future&&) = default;
    task_future(bool sync__, std::future<result_type>&& f_):
        sync_{sync__},
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

    func_ptr_type f;
    std::tuple<Args...> args;
    std::promise<result_type> task_promise;

public:
    using future_type = task_future<result_type>;

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
    auto get_future(bool sync = true){
        return future_type{sync, task_promise.get_future()};
    }
};


template<typename> class thread_pool;

//specialization for function pointer
template<typename R, typename...Args>
class thread_pool<R(Args...)>
{
    using func_ptr_type = R(*)(Args...);
    using task_type = task<R,Args...>;
    using queue_type = mpmc_bounded_queue::st_bounded_queue<task_type>;
    using mutex_type = std::mutex;

public:
    using future_type = typename task_type::future_type;

    //?exception if push task after stop
    ~thread_pool()
    {
        stop();
    }
    thread_pool(std::size_t n_workers, std::size_t n_tasks = n_workers):
        workers(n_workers),
        tasks(n_tasks)
    {
        init();
    }

    //async version returns task_future that not wait for task complete in destructor, but still possible to call wait() explicitly
    template<typename...Args_>
    auto try_push(func_ptr_type f, Args_&&...args){return try_push_<true>(f, std::forward<Args_>(args)...);}
    template<typename...Args_>
    auto try_push_async(func_ptr_type f, Args_&&...args){return try_push_<false>(f, std::forward<Args_>(args)...);}

    template<typename...Args_>
    auto push(func_ptr_type f, Args_&&...args){return push_<true>(f, std::forward<Args_>(args)...);}
    template<typename...Args_>
    auto push_async(func_ptr_type f, Args_&&...args){return push_<false>(f, std::forward<Args_>(args)...);}

private:

    template<bool Sync = true,  typename...Args_>
    auto try_push_(func_ptr_type f, Args_&&...args){
        std::unique_lock<mutex_type> lock{guard};
        if (auto t = tasks.try_push(f,std::forward<Args_>(args)...)){
            auto valid_task_future = t->get_future(Sync);
            lock.unlock();
            has_task.notify_one();
            return valid_task_future;
        }else{
            lock.unlock();
            return task_future<R>{};   //empty task_future
        }
    }

    template<bool Sync = true, typename...Args_>
    auto push_(func_ptr_type f, Args_&&...args){
        while(true){
            if (auto res = try_push_<Sync>(f, std::forward<Args_>(args)...)){
                return res;
            }
            std::this_thread::yield();
        }
    }

    void init(){
        std::for_each(workers.begin(),workers.end(),[this](auto& worker){worker=std::thread{&thread_pool::worker_loop, this};});
    }

    void stop(){
        std::unique_lock<mutex_type> lock{guard};
        finish_workers.store(true);
        lock.unlock();
        has_task.notify_all();
        std::for_each(workers.begin(),workers.end(),[](auto& worker){worker.join();});
    }

    //problem is to use waiting not yealding in loop and have concurrent push and pop
    //conditional_variable must use same mutex to guard push and pop, even if queue is mpmc
    void worker_loop(){
        while(!finish_workers.load()){  //worker loop
            std::unique_lock<mutex_type> lock{guard};
            while(!finish_workers.load()){  //has_task conditional loop
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

    std::vector<std::thread> workers;
    queue_type tasks;
    std::atomic<bool> finish_workers{false};
    mutex_type guard;
    std::condition_variable has_task;
};


}   //end of namespace experimental_multithreading

#endif