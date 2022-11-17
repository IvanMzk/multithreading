#ifndef SPINLOCK_MUTEX_HPP_
#define SPINLOCK_MUTEX_HPP_

#include <atomic>


namespace experimental_multithreading{

class spinlock_mutex
{
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
public:
    void lock(){
        while(flag.test_and_set(std::memory_order_acquire));
    }

    void unlock(){
        flag.clear(std::memory_order_release);
    }

};

}   //end of namespace experimental_multithreading

#endif