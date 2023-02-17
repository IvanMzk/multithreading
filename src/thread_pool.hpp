#ifndef THREAD_POOL_HPP_
#define THREAD_POOL_HPP_
#include <iostream>
#include <type_traits>
#include <functional>
#include <array>
#include <tuple>
#include <thread>
#include <mutex>
#include <future>
#include <condition_variable>
#include "queue.hpp"

namespace thread_pool{

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

class task_v3_base
{
public:
    virtual ~task_v3_base(){}
    virtual void call() = 0;
};

template<typename F, typename...Args>
class task_v3_impl : public task_v3_base
{
    using result_type = std::invoke_result_t<F,Args...>;
    F f;
    std::tuple<Args...> args;
    std::promise<result_type> task_promise;
    void call() override {
            if constexpr(std::is_void_v<result_type>){
                std::apply(f, std::move(args));
                task_promise.set_value();
            }else{
                task_promise.set_value(std::apply(f, std::move(args)));
            }
        }
public:
    using future_type = task_future<result_type>;
    template<typename F_, typename...Args_>
    task_v3_impl(F_&& f_, Args_&&...args_):
            f{std::forward<F_>(f_)},
            args{std::forward<Args_>(args_)...}
        {}
    auto get_future(bool sync = true){
            return future_type{sync, task_promise.get_future()};
        }
};

class task_v3
{
    std::unique_ptr<task_v3_base> impl;
public:
    task_v3() = default;
    void call(){
        impl->call();
    }
    template<typename F, typename...Args>
    auto set_task(bool sync, F&& f, Args&&...args){
        using impl_type = task_v3_impl<std::decay_t<F>, std::decay_t<Args>...>;
        impl = std::make_unique<impl_type>(std::forward<F>(f),std::forward<Args>(args)...);
        return static_cast<impl_type*>(impl.get())->get_future(sync);
    }
};


/*
* allocation free thread pool with bounded task queue
* fixed signature and return type of task callable function
* thread_pool_v1 has waiting worker loop and so may have bigger response time than v2
* which has yielding worker loop and smaller response time
*/

template<typename> class thread_pool_v1;
template<typename R, typename...Args>
class thread_pool_v1<R(Args...)>
{
    using func_ptr_type = R(*)(Args...);
    using task_type = task<R,Args...>;
    using queue_type = queue::st_bounded_queue<task_type>;
    using mutex_type = std::mutex;

public:
    using future_type = typename task_type::future_type;

    ~thread_pool_v1()
    {
        stop();
    }
    thread_pool_v1(std::size_t n_workers, std::size_t n_tasks):
        workers(n_workers),
        tasks(n_tasks)
    {
        init();
    }

    //async version returns task_future that not wait for task complete in destructor, but still possible to call wait() explicitly
    template<typename...Args_>
    auto push(func_ptr_type f, Args_&&...args){return push_<true>(f, std::forward<Args_>(args)...);}
    template<typename...Args_>
    auto push_async(func_ptr_type f, Args_&&...args){return push_<false>(f, std::forward<Args_>(args)...);}

private:
    template<bool Sync = true,  typename...Args_>
    auto push_(func_ptr_type f, Args_&&...args){
        std::unique_lock<mutex_type> lock{guard};
        while(true){
            if (auto task = tasks.try_push(f,std::forward<Args_>(args)...)){
                auto future = task->get_future(Sync);
                lock.unlock();
                has_task.notify_one();
                return future;
            }else{
                has_slot.wait(lock);
            }
        }
    }

    void init(){
        std::for_each(workers.begin(),workers.end(),[this](auto& worker){worker=std::thread{&thread_pool_v1::worker_loop, this};});
    }

    void stop(){
        std::unique_lock<mutex_type> lock{guard};
        finish_workers.store(true);
        has_task.notify_all();
        lock.unlock();
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
                    has_slot.notify_one();
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
    mutex_type guard{};
    std::condition_variable has_task{};
    std::condition_variable has_slot{};
};

template<typename> class thread_pool_v2;
template<typename R, typename...Args>
class thread_pool_v2<R(Args...)>
{
    using func_ptr_type = R(*)(Args...);
    using task_type = task<R,Args...>;
    using queue_type = queue::mpmc_bounded_queue_v1<task_type>;

public:
    using future_type = typename task_type::future_type;

    ~thread_pool_v2()
    {
        stop();
    }
    thread_pool_v2(std::size_t n_workers, std::size_t n_tasks):
        workers(n_workers),
        tasks(n_tasks)
    {
        init();
    }

    //async version returns task_future that not wait for task complete in destructor, but still possible to call wait() explicitly
    template<typename...Args_>
    auto push(func_ptr_type f, Args_&&...args){return push_<true>(f, std::forward<Args_>(args)...);}
    template<typename...Args_>
    auto push_async(func_ptr_type f, Args_&&...args){return push_<false>(f, std::forward<Args_>(args)...);}

private:

    template<bool Sync = true, typename...Args_>
    auto push_(func_ptr_type f, Args_&&...args){
        task_type task{f, std::forward<Args_>(args)...};
        auto future = task.get_future(Sync);
        tasks.push(std::move(task));
        return future;
    }

    void init(){
        std::for_each(workers.begin(),workers.end(),[this](auto& worker){worker=std::thread{&thread_pool_v2::worker_loop, this};});
    }

    void stop(){
        finish_workers.store(true);
        std::for_each(workers.begin(),workers.end(),[](auto& worker){worker.join();});
    }

    void worker_loop(){
        while(!finish_workers.load()){  //worker loop
            if(auto t = tasks.try_pop()){
                t.get().call();
            }else{
                std::this_thread::yield();
            }
        }
    }

    std::vector<std::thread> workers;
    queue_type tasks;
    std::atomic<bool> finish_workers{false};
};


//single allocation thread pool with bounded task queue
//allow different signatures and return types of task callable
//push task template method returns task_future<R>, where R is return type of callable given arguments types
class thread_pool_v3
{
    using task_type = task_v3;
    using queue_type = queue::st_bounded_queue<task_type>;
    using mutex_type = std::mutex;

public:

    ~thread_pool_v3()
    {
        stop();
    }
    thread_pool_v3(std::size_t n_workers):
        thread_pool_v3(n_workers, n_workers)
    {}
    thread_pool_v3(std::size_t n_workers, std::size_t n_tasks):
        workers(n_workers),
        tasks(n_tasks)
    {
        init();
    }

    //return task_future<R>, where R is return type of F called with args
    template<typename F, typename...Args>
    auto push(F&& f, Args&&...args){return push_<true>(std::forward<F>(f), std::forward<Args>(args)...);}
    template<typename F, typename...Args>
    auto push_async(F&& f, Args&&...args){return push_<false>(std::forward<F>(f), std::forward<Args>(args)...);}

private:

    template<bool Sync = true, typename F, typename...Args>
    auto push_(F&& f, Args&&...args){
        using future_type = decltype( std::declval<task_type>().set_task(Sync, std::forward<F>(f), std::forward<Args>(args)...));
        std::unique_lock<mutex_type> lock{guard};
        while(true){
            if (auto task = tasks.try_push()){
                future_type future = task->set_task(Sync, std::forward<F>(f), std::forward<Args>(args)...);
                lock.unlock();
                has_task.notify_one();
                return future;
            }else{
                has_slot.wait(lock);
            }
        }
    }

    void init(){
        std::for_each(workers.begin(),workers.end(),[this](auto& worker){worker=std::thread{&thread_pool_v3::worker_loop, this};});
    }

    void stop(){
        std::unique_lock<mutex_type> lock{guard};
        finish_workers.store(true);
        has_task.notify_all();
        lock.unlock();
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
                    has_slot.notify_one();
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
    std::condition_variable has_slot;
};

class thread_pool_v4
{
    using task_base_type = task_v3_base;
    using queue_type = queue::st_queue_of_polymorphic<task_base_type>;
    using mutex_type = std::mutex;

public:

    ~thread_pool_v4()
    {
        stop();
    }
    thread_pool_v4(std::size_t n_workers):
        workers(n_workers)
    {
        init();
    }

    //return task_future<R>, where R is return type of F called with args
    template<typename F, typename...Args>
    auto push(F&& f, Args&&...args){return push_<true>(std::forward<F>(f), std::forward<Args>(args)...);}
    template<typename F, typename...Args>
    auto push_async(F&& f, Args&&...args){return push_<false>(std::forward<F>(f), std::forward<Args>(args)...);}

private:

    template<bool Sync = true, typename F, typename...Args>
    auto push_(F&& f, Args&&...args){
        using task_impl_type = task_v3_impl<std::decay_t<F>, std::decay_t<Args>...>;
        using future_type = typename task_impl_type::future_type;
        std::unique_lock<mutex_type> lock{guard};
        auto task = tasks.push<task_impl_type>(std::forward<F>(f), std::forward<Args>(args)...);
        future_type future = task->get_future(Sync);
        has_task.notify_one();
        lock.unlock();
        return future;
    }

    void init(){
        std::for_each(workers.begin(),workers.end(),[this](auto& worker){worker=std::thread{&thread_pool_v4::worker_loop, this};});
    }

    void stop(){
        std::unique_lock<mutex_type> lock{guard};
        finish_workers.store(true);
        has_task.notify_all();
        lock.unlock();
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
                    t->call();
                    break;
                }else{
                    has_task.wait(lock);
                }
            }
        }
    }

    std::vector<std::thread> workers;
    queue_type tasks{};
    std::atomic<bool> finish_workers{false};
    mutex_type guard;
    std::condition_variable has_task;
};

}   //end of namespace thread_pool

#endif