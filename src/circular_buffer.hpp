#ifndef CIRCULAR_BUFFER_HPP_
#define CIRCULAR_BUFFER_HPP_

#include <array>
#include <atomic>
#include <mutex>
#include <sstream>
#include <iterator>
#include <algorithm>

namespace experimental_multithreading{

template<typename T, std::size_t N>
class spsc_circular_buffer
{
public:
    static constexpr std::size_t buffer_size = N;
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
    static constexpr std::size_t buffer_size = N;
    using mutex_type = std::mutex;
    using size_type = std::size_t;
    using value_type = T;
    mpmc_circular_buffer() = default;

    bool try_push(const value_type& v){
        std::unique_lock<mutex_type> lock{push_guard};
        auto push_index__ = push_index_.load();
        bool is_full{index(push_index__+1)==pop_index_.load()};
        if (!is_full){
            elements_[push_index__] = v;
            push_index_.store(index(push_index__+1));
            return true;
        }else{
            return false;
        }
    }

    bool try_pop(value_type& v){
        std::unique_lock<mutex_type> lock{pop_guard};
        auto pop_index__ = pop_index_.load();
        bool is_empty{push_index_.load() == pop_index__};
        if (!is_empty){
            v = elements_[pop_index_];
            pop_index_.store(index(pop_index__+1));
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
        auto push_index__ = push_index_.load();
        auto pop_index__ = pop_index_.load();
        return pop_index__ > push_index__ ? (N+1+push_index__-pop_index__) : (push_index__ - pop_index__);
    }

private:
    auto index(size_type c)const{return c%(N+1);}

    std::array<value_type,N+1> elements_;
    std::atomic<size_type> push_index_;
    std::atomic<size_type> pop_index_;
    mutex_type push_guard;
    mutex_type pop_guard;
};

template<typename T, std::size_t N>
class mpmc_lock_free_circular_buffer
{
public:
    static constexpr std::size_t buffer_size = N;
    using size_type = std::size_t;
    using value_type = T;
    mpmc_lock_free_circular_buffer() = default;

    bool try_push(const value_type& v){
        if (!debug_stop_.load()){
            auto push_counter__ = push_counter_.load();
            auto& element = elements_[index(push_counter__)];
            auto expected_state = state(push_counter__);
            if (element.state.load() == expected_state){ //buffer overwrite protection
                if (push_counter_.compare_exchange_weak(push_counter__, push_counter__+1)){
                    element.value = v;
                    element.state.store(expected_state+2);
                    return true;
                }else{
                    return false;
                }
            }else{
                return false;
            }

        }else{
            return false;
        }
    }

    bool try_pop(value_type& v){
        if (!debug_stop_.load()){
            auto pop_counter__ = pop_counter_.load();
            auto& element = elements_[index(pop_counter__)];
            auto expected_state = state(pop_counter__)+2;
            if (element.state.load() == expected_state){
                if (pop_counter_.compare_exchange_weak(pop_counter__, pop_counter__+1)){
                    v = element.value;
                    element.state.store(expected_state-1);
                    return true;
                }else{
                    return false;
                }
            }else{
                return false;
            }

        }else{
            return false;
        }
    }

    void push(const value_type& v){

    }

    auto pop(){

    }

    auto size()const{return push_counter_.load() - pop_counter_.load();}

    void debug_stop(){
        debug_stop_.store(true);
    }
    void debug_start(){
        debug_stop_.store(false);
    }
    auto debug_to_str(){
        auto res = std::stringstream{};
        res<<std::endl<<push_counter_.load()<<" "<<index(push_counter_.load())<<" "<<pop_counter_.load()<<" "<<index(pop_counter_.load())<<std::endl;
        for (const element_type& i : elements_){
            res<<i.value<<" "<<i.state.load()<<",";
        }
        return res.str();
    }

private:
    // enum class element_state_type:std::size_t{empty, locked_for_push, locked_for_pop, full};
    // friend std::ostream& operator<<(std::ostream& os, const element_state_type& s){
    //     switch (s){
    //         case element_state_type::empty:
    //             return os<<"empty";
    //         case element_state_type::locked_for_push:
    //             return os<<"locked_for_push";
    //         case element_state_type::locked_for_pop:
    //             return os<<"locked_for_pop";
    //         case element_state_type::full:
    //             return os<<"full";
    //     }
    // }

    struct element_type
    {
        element_type():
            value{},
            state{0}
        {}
        value_type value;
        std::atomic<std::size_t> state;
    };

    auto index(size_type c)const{return c%(N);}
    auto state(size_type c)const{return c/(N);}

    std::array<element_type,N> elements_;
    std::atomic<size_type> push_counter_;
    std::atomic<size_type> pop_counter_;
    std::atomic<bool> debug_stop_{false};
};

}   //end of namespace experimental_multithreading


#endif