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
        
    }
    
    auto size()const{
        return end_;
    }
    auto operator[](std::size_t i){
        return elements_[i]; 
    }
private:
    mutex_type guard;
    std::array<value_type,N> elements_{};
    std::size_t end_{0};
};

// template<typename T, std::size_t N>
// class static_stack
// {
// public:
//     using value_type = T;
//     static_stack() = default;
//     auto push(const value_type& v){
//         auto expected_end = end_.load();
//         while(true){
//             elements_[expected_end] = v;
//             if (end_.compare_exchange_weak(expected_end,expected_end+1)){
//                 break;
//             }
//         }
//     }
//     auto pop(){
//         auto expected_end = end_.load();
//         while(true){
//             auto v = elements_[expected_end-1];
//             if (end_.compare_exchange_weak(expected_end,expected_end-1)){
//                 return v;
//             }
//         }
//     }
//     // race with push
//     // auto pop(){
//     //     auto old_end = end_.fetch_sub(1);
//     //     return elements_[old_end-1];
//     // }
//     auto size()const{
//         return end_.load();
//     }
//     auto operator[](std::size_t i){
//         return elements_[i]; 
//     }
// private:
//     std::array<value_type,N> elements_{};
//     std::atomic<std::size_t> end_{0};
// };


}   //end of namespace experimental_multithreading

#endif