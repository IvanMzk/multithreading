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

/*
* use "epoch number" that is counter/buffer_size as protector from ABA
* not safe under counter overflow
*/
template<typename T, std::size_t N, typename SizeT = std::size_t>
class mpmc_lock_free_circular_buffer_v1
{
public:
    static constexpr std::size_t buffer_size = N;
    using size_type = SizeT;
    using value_type = T;
    mpmc_lock_free_circular_buffer_v1() = default;

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
        for (const element& i : elements_){
            res<<i.value<<" "<<i.state.load()<<",";
        }
        return res.str();
    }

private:
    struct element
    {
        element():
            value{},
            state{0}
        {}
        value_type value;
        std::atomic<std::size_t> state;
    };

    auto index(size_type c)const{return c%(N);}
    auto state(size_type c)const{return c/(N);}

    std::array<element,N> elements_;
    std::atomic<size_type> push_counter_;
    std::atomic<size_type> pop_counter_;
    std::atomic<bool> debug_stop_{false};
};

template<typename T, std::size_t N, typename SizeT = std::size_t>
class mpmc_lock_free_circular_buffer_v2
{
public:
    static constexpr std::size_t buffer_size = N;
    using size_type = SizeT;
    using value_type = T;
    static_assert(std::numeric_limits<size_type>::max() >= N);

    mpmc_lock_free_circular_buffer_v2():
        elements_{make_elements(std::make_integer_sequence<size_type, buffer_size>{})},
        push_counter_{0},
        pop_counter_{0}
    {}

    bool try_push(const value_type& v){
        if (!debug_stop_.load()){
            auto push_counter__ = push_counter_.load();
            auto& element = elements_[index(push_counter__)];
            if (element.id.load() == push_counter__){ //buffer overwrite protection
                if (push_counter_.compare_exchange_weak(push_counter__, static_cast<size_type>(push_counter__+1))){
                    element.value = v;
                    element.id.store(static_cast<size_type>(push_counter__+1));
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
            if (element.id.load() == static_cast<size_type>(pop_counter__+1)){
                if (pop_counter_.compare_exchange_weak(pop_counter__, static_cast<size_type>(pop_counter__+1))){
                    v = element.value;
                    element.id.store(static_cast<size_type>(pop_counter__+buffer_size));
                    return true;
                }else{
                    // if (pop_counter__ == 255){
                    //     std::cout<<std::endl<<"if (pop_counter_.compare_exchange_weak(pop_counter__, pop_counter__+1)){";
                    // }
                    return false;
                }
            }else{
                // if (pop_counter__ == 255){
                //     std::cout<<std::endl<<"if (element.id.load() == pop_counter__+1){"<<static_cast<std::size_t>(element.id.load())<<static_cast<std::size_t>(pop_counter__)<<
                //         typeid(pop_counter__).name()<<typeid(size_type{1}).name()<<typeid(pop_counter__+size_type{1}).name()<<typeid(cc+ccc).name();
                // }
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
        res<<std::endl<<static_cast<std::size_t>(push_counter_.load())<<" "<<index(push_counter_.load())<<" "<<
            static_cast<std::size_t>(pop_counter_.load())<<" "<<index(pop_counter_.load())<<std::endl;
        for (const element& i : elements_){
            res<<i.value<<" "<<static_cast<std::size_t>(i.id.load())<<",";
        }
        return res.str();
    }

private:
    struct element
    {
        element(size_type i):
            value{},
            id{i}
        {}
        value_type value;
        std::atomic<size_type> id;
    };

    template<size_type I>
    auto make_element()const{return element{I};}
    template<size_type...I>
    auto make_elements(std::integer_sequence<size_type, I...>){return std::array<element, sizeof...(I)>{make_element<I>()...};}

    auto index(size_type c)const{return c%(buffer_size);}

    std::array<element,buffer_size> elements_;
    std::atomic<size_type> push_counter_;
    std::atomic<size_type> pop_counter_;
    std::atomic<bool> debug_stop_{false};
};

}   //end of namespace experimental_multithreading


#endif