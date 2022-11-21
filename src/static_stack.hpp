#ifndef STATIC_STACK_HPP_
#define STATIC_STACK_HPP_
#include <atomic>
#include <mutex>
#include <array>

namespace experimental_multithreading{

template<typename T, std::size_t N, typename MutexT = std::mutex>
class static_stack
{
public:
    using mutex_type = MutexT;
    using value_type = T;

    static_stack() = default;
    auto push(const value_type& v){
        std::unique_lock<mutex_type> lock{guard};
        elements_[end_] = v;
        ++end_;
    }
    auto pop(){
        std::unique_lock<mutex_type> lock{guard};
        auto v = elements_[end_-1];
        --end_;
        return v;
    }
    auto size()const{
        return end_;
    }
private:
    mutex_type guard;
    std::array<value_type,N> elements_{};
    std::size_t end_{0};
};

}   //end of namespace experimental_multithreading

#endif