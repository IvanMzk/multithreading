#ifndef CIRCULAR_BUFFER_HPP_
#define CIRCULAR_BUFFER_HPP_

#include <array>
#include <atomic>
#include <mutex>

namespace experimental_multithreading{

template<typename T, std::size_t N>
class spsc_circular_buffer
{
public:
    using value_type = T;
    spsc_circular_buffer():
        elements_{},
        push_index{0},
        pop_index{0}
    {}

    bool try_push(const value_type& v){
        auto push_index_ = push_index.load();
        bool is_full{index(push_index_+1)==pop_index.load()};
        if (!is_full){
            elements_[push_index_] = v;
            push_index.store(index(push_index_+1));
            return true;
        }else{
            return false;
        }
    }

    bool try_pop(value_type& v){
        auto pop_index_ = pop_index.load();
        bool is_empty{push_index.load() == pop_index_};
        if (!is_empty){
            v = elements_[pop_index_];
            pop_index.store(index(pop_index_+1));
            return true;
        }else{
            return false;
        }
    }

    auto size()const{
        auto push_index_ = push_index.load();
        auto pop_index_ = pop_index.load();
        return pop_index_ > push_index_ ? (N+1+push_index_-pop_index_) : (push_index_ - pop_index_);
    }

private:
    auto index(std::size_t c){return c%(N+1);}

    std::array<value_type,N+1> elements_;
    std::atomic<std::size_t> push_index;
    std::atomic<std::size_t> pop_index;
};


template<typename T, std::size_t N>
class mpmc_circular_buffer
{
public:
    using mutex_type = std::mutex;
    using size_type = std::size_t;
    using value_type = T;
    mpmc_circular_buffer() = default;

    bool try_push(const value_type& v){
        std::unique_lock<mutex_type> lock{guard};
        bool is_full{index(push_index+1)==pop_index};
        if (!is_full){
            elements_[push_index] = v;
            push_index.store(index(push_index+1));
            return true;
        }else{
            return false;
        }
    }

    bool try_pop(value_type& v){
        std::unique_lock<mutex_type> lock{guard};
        bool is_empty{push_index == pop_index};
        if (!is_empty){
            v = elements_[pop_index];
            pop_index.store(index(pop_index+1));
            return true;
        }else{
            return false;
        }
    }

    void push(const value_type& v){

    }

    auto pop(){
        
    }

    auto size()const{
        std::unique_lock<mutex_type> lock{guard};
        return pop_index > push_index ? (N+1+push_index-pop_index) : (push_index - pop_index);
    }

private:
    auto index(size_type c){return c%(N+1);}

    std::array<value_type,N+1> elements_;
    size_type push_index;
    size_type pop_index;
    mutex_type guard;
};

}   //end of namespace experimental_multithreading


#endif