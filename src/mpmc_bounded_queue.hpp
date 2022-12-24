#ifndef MPMC_BOUNDED_QUEUE_HPP_
#define MPMC_BOUNDED_QUEUE_HPP_

#include <atomic>
#include <array>

namespace mpmc_bounded_queue{

template<typename SizeT, SizeT Capacity>
auto index_(SizeT c){
    if constexpr (static_cast<bool>(Capacity&(Capacity-1))){
        return c%Capacity;
    }else{
        return c&(Capacity-1);
    }
}

template<typename T>
class element_
{
    using value_type = T;
    alignas(value_type) std::byte buffer[sizeof(value_type)];
public:
    template<typename...Args>
    void emplace(Args&&...args){
        new(reinterpret_cast<void*>(buffer)) value_type{std::forward<Args>(args)...};
    }
    value_type&& get(){
        return std::move(*reinterpret_cast<value_type*>(buffer));
    }
    void destroy(){
        reinterpret_cast<value_type*>(buffer)->~value_type();
    }
};


template<typename T, std::size_t N>
class mpmc_bounded_queue_v1
{
    using size_type = std::size_t;
    static constexpr size_type capacity_ = N;
    static constexpr size_type(*index)(size_type) = index_<size_type, capacity_>;
    static constexpr std::size_t hardware_destructive_interference_size = std::hardware_destructive_interference_size;
    static_assert(std::is_unsigned_v<size_type>);
    static_assert(capacity_ > 1);
public:
    using value_type = T;

    mpmc_bounded_queue_v1()
    {
        static constexpr std::size_t push_counter_offset = offsetof(mpmc_bounded_queue_v1, push_counter_);
        static constexpr std::size_t pop_counter_offset = offsetof(mpmc_bounded_queue_v1, pop_counter_);
        static_assert(std::max(push_counter_offset,pop_counter_offset) - std::min(push_counter_offset,pop_counter_offset) >= hardware_destructive_interference_size);
        static_assert(alignof(element) == hardware_destructive_interference_size);
        init();
    }
    ~mpmc_bounded_queue_v1(){
        clear();
    }

    template<typename...Args>
    bool try_push(Args&&...args){
        auto push_counter__ = push_counter_.load(std::memory_order::memory_order_relaxed);
        while(true){
            auto& element = elements_[index(push_counter__)];
            auto id = element.id.load(std::memory_order::memory_order_acquire);
            if (id == push_counter__){ //buffer overwrite protection
                auto next_push_counter = push_counter__+1;
                if (push_counter_.compare_exchange_weak(push_counter__, next_push_counter,std::memory_order::memory_order_relaxed)){
                    element.emplace(std::forward<Args>(args)...);
                    element.id.store(next_push_counter, std::memory_order::memory_order_release);
                    return true;
                }
            }else if (id < push_counter__){//queue full, exit
                return false;
            }else{//element full, try next
                push_counter__ = push_counter_.load(std::memory_order::memory_order_relaxed);
            }
        }
    }

    bool try_pop(value_type& v){
        auto pop_counter__ = pop_counter_.load(std::memory_order::memory_order_relaxed);
        while(true){
            auto& element = elements_[index(pop_counter__)];
            auto next_pop_counter = pop_counter__+1;
            auto id = element.id.load(std::memory_order::memory_order_acquire);
            if (id == next_pop_counter){
                if (pop_counter_.compare_exchange_weak(pop_counter__, next_pop_counter, std::memory_order::memory_order_relaxed)){//pop_counter__ updated when fails
                    v = element.get();
                    element.id.store(pop_counter__+capacity_, std::memory_order::memory_order_release);
                    return true;
                }
            }else if (id < next_pop_counter){//queue empty, exit
                return false;
            }else{//element empty, try next
                pop_counter__ = pop_counter_.load(std::memory_order::memory_order_relaxed);
            }
        }
    }

    template<typename...Args>
    void push(Args&&...args){
        auto push_counter__ = push_counter_.fetch_add(1, std::memory_order::memory_order_relaxed);  //reserve
        auto& element = elements_[index(push_counter__)];
        while(push_counter__ != element.id.load(std::memory_order::memory_order_acquire)){};   //wait until element is empty
        element.emplace(std::forward<Args>(args)...);
        element.id.store(push_counter__+1, std::memory_order::memory_order_release);
    }

    void pop(value_type& v){
        auto pop_counter__ = pop_counter_.fetch_add(1, std::memory_order::memory_order_relaxed);    //reserve
        auto next_pop_counter = pop_counter__+1;
        auto& element = elements_[index(pop_counter__)];
        while(next_pop_counter != element.id.load(std::memory_order::memory_order_acquire)){};  //wait until element is full
        v = element.get();
        element.id.store(pop_counter__+capacity_, std::memory_order::memory_order_release);
    }

    auto size()const{return push_counter_.load(std::memory_order::memory_order_relaxed) - pop_counter_.load(std::memory_order::memory_order_relaxed);}
    constexpr size_type capacity()const{return capacity_;}

private:

    void clear(){
        auto pop_counter__ = pop_counter_.load(std::memory_order::memory_order_relaxed);
        for (std::size_t i = 0; i!=capacity_; ++i, ++pop_counter__){
            auto& element = elements_[index(pop_counter__)];
            if (element.id.load(std::memory_order::memory_order_acquire) == pop_counter__+1){
                element.destroy();
            }else{
                break;
            }
        }
    }

    class element : public element_<value_type>{
    public:
        alignas(hardware_destructive_interference_size) std::atomic<size_type> id{};
    };

    void init(){
        for (size_type i{0}; i!=capacity_; ++i){
            elements_[i].id.store(i);
        }
    }

    std::array<element,capacity_> elements_{};
    alignas(hardware_destructive_interference_size) std::atomic<size_type> push_counter_{0};
    alignas(hardware_destructive_interference_size) std::atomic<size_type> pop_counter_{0};
};

template<typename T, std::size_t N>
class mpmc_bounded_queue_v2
{
    using size_type = std::size_t;
    static constexpr size_type capacity_ = N;
    static constexpr size_type(*index)(size_type) = index_<size_type, capacity_+1>;
    static constexpr std::size_t hardware_destructive_interference_size = std::hardware_destructive_interference_size;
    static_assert(std::is_unsigned_v<size_type>);
public:
    using value_type = T;

    ~mpmc_bounded_queue_v2()
    {
        clear();
    }

    template<typename...Args>
    bool try_push(Args&&...args){
        auto push_reserve_counter__ = push_reserve_counter_.load(std::memory_order::memory_order_relaxed);
        while(true){
            if (push_reserve_counter__ - pop_counter_.load(std::memory_order::memory_order_acquire) == capacity_){  //full    //acquaire1
                return false;
            }else{
                auto next_push_reserve_counter = push_reserve_counter__+1;
                if (push_reserve_counter_.compare_exchange_weak(push_reserve_counter__, next_push_reserve_counter, std::memory_order::memory_order_relaxed)){
                    elements_[index(push_reserve_counter__)].emplace(std::forward<Args>(args)...);
                    while(push_counter_.load(std::memory_order::memory_order_acquire) != push_reserve_counter__){}//wait for prev pushes  //acquaire0
                    push_counter_.store(next_push_reserve_counter, std::memory_order::memory_order_release);     //release0
                    return true;
                }
            }
        }
    }

    bool try_pop(value_type& v){
        auto pop_reserve_counter__ = pop_reserve_counter_.load(std::memory_order::memory_order_relaxed);
        while(true){
            if (push_counter_.load(std::memory_order::memory_order_acquire) == pop_reserve_counter__){   //empty   //acquaire0
                return false;
            }else{
                auto next_pop_reserve_counter = pop_reserve_counter__+1;
                if (pop_reserve_counter_.compare_exchange_weak(pop_reserve_counter__, next_pop_reserve_counter, std::memory_order::memory_order_relaxed)){
                    v = elements_[index(pop_reserve_counter__)].get();
                    while(pop_counter_.load(std::memory_order::memory_order_acquire) != pop_reserve_counter__){}//wait for prev pops  //acquaire1
                    pop_counter_.store(next_pop_reserve_counter, std::memory_order::memory_order_release);   //release1
                    return true;
                }
            }
        }
    }

    template<typename...Args>
    void push(Args&&...args){
        auto push_reserve_counter__ = push_reserve_counter_.fetch_add(1, std::memory_order::memory_order_relaxed);   //leads to overwrite
        while(push_reserve_counter__ - pop_counter_.load(std::memory_order::memory_order_acquire) >= capacity_){}    //wait until not full
        elements_[index(push_reserve_counter__)].emplace(std::forward<Args>(args)...);
        while(push_counter_.load(std::memory_order::memory_order_acquire) != push_reserve_counter__){}     //wait for prev pushes
        push_counter_.store(push_reserve_counter__+1, std::memory_order::memory_order_release); //commit
    }

    void pop(value_type& v){
        auto pop_reserve_counter__ = pop_reserve_counter_.fetch_add(1, std::memory_order::memory_order_relaxed);
        while(pop_reserve_counter__ >= push_counter_.load(std::memory_order::memory_order_acquire)){}   //wait until not empty
        v = elements_[index(pop_reserve_counter__)].get();
        while(pop_counter_.load(std::memory_order::memory_order_acquire) != pop_reserve_counter__){}   //wait for prev pops
        pop_counter_.store(pop_reserve_counter__+1, std::memory_order::memory_order_release);    //commit
    }

    auto size()const{return push_counter_.load(std::memory_order::memory_order_relaxed) - pop_counter_.load(std::memory_order::memory_order_relaxed);}
    constexpr size_type capacity()const{return capacity_;}

private:

    void clear(){
        auto pop_reserve_counter__ = pop_reserve_counter_.load(std::memory_order::memory_order_relaxed);
        auto push_counter__ = push_counter_.load(std::memory_order::memory_order_acquire);
        while(push_counter__ != pop_reserve_counter__){
            elements_[index(pop_reserve_counter__)].destroy();
            ++pop_reserve_counter__;
        }
    }

    std::array<element_<value_type>, capacity_+1> elements_{};
    alignas(hardware_destructive_interference_size) std::atomic<size_type> push_counter_{0};
    alignas(hardware_destructive_interference_size) std::atomic<size_type> push_reserve_counter_{0};
    alignas(hardware_destructive_interference_size) std::atomic<size_type> pop_counter_{0};
    alignas(hardware_destructive_interference_size) std::atomic<size_type> pop_reserve_counter_{0};
};


}   //end of namespace mpmc_bounded_queue

#endif