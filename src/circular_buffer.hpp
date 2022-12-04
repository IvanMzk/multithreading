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
* use "epoch number" that is counter/buffer_size as protector from overwriting and read before write
* not safe under counter overflow
*/
template<typename T, std::size_t N, typename SizeT = std::size_t>
class mpmc_lock_free_circular_buffer_v1
{
public:
    using size_type = SizeT;
    using value_type = T;
    static constexpr std::size_t buffer_size = N;
    static constexpr std::size_t max_state = std::numeric_limits<size_type>::max()/buffer_size;
    mpmc_lock_free_circular_buffer_v1() = default;

    bool try_push(const value_type& v){
        if (!debug_stop_.load()){
            auto push_counter__ = push_counter_.load();
            auto& element = elements_[index(push_counter__)];
            auto expected_state = state(push_counter__);
            if (element.state.load() == expected_state){ //buffer overwrite protection
                if (push_counter_.compare_exchange_weak(push_counter__, static_cast<size_type>(push_counter__+1))){
                    element.value = v;
                    element.state.store(static_cast<size_type>(expected_state+2));
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
            auto expected_state = static_cast<size_type>(state(pop_counter__))+2;
            if (element.state.load() == expected_state){
                if (pop_counter_.compare_exchange_weak(pop_counter__, pop_counter__+1)){
                    v = element.value;
                    element.state.store(state(pop_counter__) == max_state ? 0 : expected_state-1);//must set to 0 if overflow in this epoch
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
        std::atomic<size_type> state;
    };

    auto index(size_type c)const{return c%(buffer_size);}
    auto state(size_type c)const{return static_cast<size_type>(c/buffer_size);}

    std::array<element,N> elements_;
    std::atomic<size_type> push_counter_;
    std::atomic<size_type> pop_counter_;
    std::atomic<bool> debug_stop_{false};
};


/*
* if buffer_size is power of two counter overflow is safe, can use any unsigned size_type
* if buffer_size is not power of two counter overflow leads to deadlock, should use the largest possible atomic type
*/
template<typename T, std::size_t N, typename SizeT = std::size_t>
class mpmc_lock_free_circular_buffer_v2
{
    static constexpr SizeT size_type_max{std::numeric_limits<SizeT>::max()};
    static constexpr SizeT counter_max{static_cast<SizeT>(size_type_max%N == N-1 ? size_type_max : (size_type_max/N)*N - 1)};
    static_assert(counter_max > N-1);
    static_assert(std::is_unsigned_v<SizeT>);
public:
    using size_type = SizeT;
    using value_type = T;
    static constexpr size_type buffer_size = N;

    mpmc_lock_free_circular_buffer_v2()
    {
        init();
    }

    bool try_push(const value_type& v){
        if (!debug_stop_.load()){
            auto push_counter__ = push_counter_.load();
            auto& element = elements_[index(push_counter__)];
            if (element.id.load() == push_counter__){ //buffer overwrite protection
                auto new_push_counter = counter_inc(push_counter__);
                if (push_counter_.compare_exchange_weak(push_counter__, new_push_counter)){
                    element.value = v;
                    element.id.store(new_push_counter);
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
            auto new_pop_counter{counter_inc(pop_counter__)};
            if (element.id.load() == new_pop_counter){
                if (pop_counter_.compare_exchange_weak(pop_counter__, new_pop_counter)){
                    v = element.value;
                    element.id.store(counter_add_buffer_size(pop_counter__));
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
    size_type(*counter_inc)(size_type) = counter_inc_<counter_max>;
    size_type(*counter_add_buffer_size)(size_type) = counter_add_buffer_size_<counter_max>;

    template<size_type CounterMax>
    static auto counter_inc_(size_type c){
        return static_cast<size_type>(c==CounterMax ? 0 : c+1);
    }
    template<>
    static auto counter_inc_<size_type_max>(size_type c){
        return static_cast<size_type>(c+1);
    }
    template<size_type CounterMax>
    static auto counter_add_buffer_size_(size_type c){
        auto d = CounterMax - c;
        return static_cast<size_type>(d < buffer_size ? buffer_size-1-d : c+buffer_size);
    }
    template<>
    static auto counter_add_buffer_size_<size_type_max>(size_type c){
        return static_cast<size_type>(c+buffer_size);
    }

    struct element
    {
        value_type value{};
        std::atomic<size_type> id{};
    };

    void init(){
        size_type i{0};
        while(true){
            elements_[i].id.store(i);
            if (i==buffer_size-1){
                break;
            }
            ++i;
        }
    }

    auto index(size_type c)const{return c%(buffer_size);}

    std::array<element,buffer_size> elements_{};
    std::atomic<size_type> push_counter_{0};
    std::atomic<size_type> pop_counter_{0};
    std::atomic<bool> debug_stop_{false};
};

}   //end of namespace experimental_multithreading


#endif