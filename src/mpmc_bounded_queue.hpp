#ifndef MPMC_BOUNDED_QUEUE_HPP_
#define MPMC_BOUNDED_QUEUE_HPP_

#include <atomic>
#include <mutex>
#include <array>

namespace mpmc_bounded_queue{

namespace detail{
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

}   //end of namespace detail


template<typename T, std::size_t N>
class mpmc_bounded_queue_v1
{
    using size_type = std::size_t;
    static constexpr size_type capacity_ = N;
    static constexpr size_type(*index)(size_type) = detail::index_<size_type, capacity_>;
    static constexpr std::size_t hardware_destructive_interference_size = std::hardware_destructive_interference_size;
    static_assert(std::is_unsigned_v<size_type>);
    static_assert(capacity_ > 1);
public:
    using value_type = T;

    mpmc_bounded_queue_v1()
    {
        init();
    }
    ~mpmc_bounded_queue_v1()
    {
        clear();
    }

    template<typename...Args>
    bool try_push(Args&&...args){
        auto push_counter_ = push_counter.load(std::memory_order::memory_order_relaxed);
        while(true){
            auto& element = elements[index(push_counter_)];
            auto id = element.id.load(std::memory_order::memory_order_acquire);
            if (id == push_counter_){ //buffer overwrite protection
                auto next_push_counter = push_counter_+1;
                if (push_counter.compare_exchange_weak(push_counter_, next_push_counter,std::memory_order::memory_order_relaxed)){
                    element.emplace(std::forward<Args>(args)...);
                    element.id.store(next_push_counter, std::memory_order::memory_order_release);
                    return true;
                }
            }else if (id < push_counter_){//queue full, exit
                return false;
            }else{//element full, try next
                push_counter_ = push_counter.load(std::memory_order::memory_order_relaxed);
            }
        }
    }

    bool try_pop(value_type& v){
        auto pop_counter_ = pop_counter.load(std::memory_order::memory_order_relaxed);
        while(true){
            auto& element = elements[index(pop_counter_)];
            auto next_pop_counter = pop_counter_+1;
            auto id = element.id.load(std::memory_order::memory_order_acquire);
            if (id == next_pop_counter){
                if (pop_counter.compare_exchange_weak(pop_counter_, next_pop_counter, std::memory_order::memory_order_relaxed)){//pop_counter_ updated when fails
                    v = element.get();
                    element.id.store(pop_counter_+capacity_, std::memory_order::memory_order_release);
                    return true;
                }
            }else if (id < next_pop_counter){//queue empty, exit
                return false;
            }else{//element empty, try next
                pop_counter_ = pop_counter.load(std::memory_order::memory_order_relaxed);
            }
        }
    }

    template<typename...Args>
    void push(Args&&...args){
        auto push_counter_ = push_counter.fetch_add(1, std::memory_order::memory_order_relaxed);  //reserve
        auto& element = elements[index(push_counter_)];
        while(push_counter_ != element.id.load(std::memory_order::memory_order_acquire)){};   //wait until element is empty
        element.emplace(std::forward<Args>(args)...);
        element.id.store(push_counter_+1, std::memory_order::memory_order_release);
    }

    void pop(value_type& v){
        auto pop_counter_ = pop_counter.fetch_add(1, std::memory_order::memory_order_relaxed);    //reserve
        auto next_pop_counter = pop_counter_+1;
        auto& element = elements[index(pop_counter_)];
        while(next_pop_counter != element.id.load(std::memory_order::memory_order_acquire)){};  //wait until element is full
        v = element.get();
        element.id.store(pop_counter_+capacity_, std::memory_order::memory_order_release);
    }

    auto size()const{return push_counter.load(std::memory_order::memory_order_relaxed) - pop_counter.load(std::memory_order::memory_order_relaxed);}
    constexpr size_type capacity()const{return capacity_;}

private:

    void clear(){
        auto pop_counter_ = pop_counter.load(std::memory_order::memory_order_relaxed);
        for (std::size_t i = 0; i!=capacity_; ++i, ++pop_counter_){
            auto& element = elements[index(pop_counter_)];
            if (element.id.load(std::memory_order::memory_order_relaxed) == pop_counter_+1){
                element.destroy();
            }else{
                break;
            }
        }
    }

    class element : public detail::element_<value_type>{
    public:
        alignas(hardware_destructive_interference_size) std::atomic<size_type> id{};
    };

    void init(){
        for (size_type i{0}; i!=capacity_; ++i){
            elements[i].id.store(i);
        }
    }

    std::array<element,capacity_> elements{};
    alignas(hardware_destructive_interference_size) std::atomic<size_type> push_counter{0};
    alignas(hardware_destructive_interference_size) std::atomic<size_type> pop_counter{0};
};

template<typename T, std::size_t N>
class mpmc_bounded_queue_v2
{
    using size_type = std::size_t;
    static constexpr size_type capacity_ = N;
    static constexpr size_type(*index)(size_type) = detail::index_<size_type, capacity_+1>;
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
        auto push_reserve_counter_ = push_reserve_counter.load(std::memory_order::memory_order_relaxed);
        while(true){
            if (push_reserve_counter_ - pop_counter.load(std::memory_order::memory_order_acquire) == capacity_){  //full    //acquaire1
                return false;
            }else{
                auto next_push_reserve_counter = push_reserve_counter_+1;
                if (push_reserve_counter.compare_exchange_weak(push_reserve_counter_, next_push_reserve_counter, std::memory_order::memory_order_relaxed)){
                    elements[index(push_reserve_counter_)].emplace(std::forward<Args>(args)...);
                    while(push_counter.load(std::memory_order::memory_order_acquire) != push_reserve_counter_){}//wait for prev pushes  //acquaire0
                    push_counter.store(next_push_reserve_counter, std::memory_order::memory_order_release);     //release0
                    return true;
                }
            }
        }
    }

    bool try_pop(value_type& v){
        auto pop_reserve_counter_ = pop_reserve_counter.load(std::memory_order::memory_order_relaxed);
        while(true){
            if (push_counter.load(std::memory_order::memory_order_acquire) == pop_reserve_counter_){   //empty   //acquaire0
                return false;
            }else{
                auto next_pop_reserve_counter = pop_reserve_counter_+1;
                if (pop_reserve_counter.compare_exchange_weak(pop_reserve_counter_, next_pop_reserve_counter, std::memory_order::memory_order_relaxed)){
                    v = elements[index(pop_reserve_counter_)].get();
                    while(pop_counter.load(std::memory_order::memory_order_acquire) != pop_reserve_counter_){}//wait for prev pops  //acquaire1
                    pop_counter.store(next_pop_reserve_counter, std::memory_order::memory_order_release);   //release1
                    return true;
                }
            }
        }
    }

    template<typename...Args>
    void push(Args&&...args){
        auto push_reserve_counter_ = push_reserve_counter.fetch_add(1, std::memory_order::memory_order_relaxed);   //leads to overwrite
        while(push_reserve_counter_ - pop_counter.load(std::memory_order::memory_order_acquire) >= capacity_){}    //wait until not full
        elements[index(push_reserve_counter_)].emplace(std::forward<Args>(args)...);
        while(push_counter.load(std::memory_order::memory_order_acquire) != push_reserve_counter_){}     //wait for prev pushes
        push_counter.store(push_reserve_counter_+1, std::memory_order::memory_order_release); //commit
    }

    void pop(value_type& v){
        auto pop_reserve_counter_ = pop_reserve_counter.fetch_add(1, std::memory_order::memory_order_relaxed);
        while(pop_reserve_counter_ >= push_counter.load(std::memory_order::memory_order_acquire)){}   //wait until not empty
        v = elements[index(pop_reserve_counter_)].get();
        while(pop_counter.load(std::memory_order::memory_order_acquire) != pop_reserve_counter_){}   //wait for prev pops
        pop_counter.store(pop_reserve_counter_+1, std::memory_order::memory_order_release);    //commit
    }

    auto size()const{return push_counter.load(std::memory_order::memory_order_relaxed) - pop_counter.load(std::memory_order::memory_order_relaxed);}
    constexpr size_type capacity()const{return capacity_;}

private:

    void clear(){
        auto pop_reserve_counter_ = pop_reserve_counter.load(std::memory_order::memory_order_relaxed);
        auto push_counter_ = push_counter.load(std::memory_order::memory_order_relaxed);
        while(push_counter_ != pop_reserve_counter_){
            elements[index(pop_reserve_counter_)].destroy();
            ++pop_reserve_counter_;
        }
    }

    std::array<detail::element_<value_type>, capacity_+1> elements{};
    alignas(hardware_destructive_interference_size) std::atomic<size_type> push_counter{0};
    alignas(hardware_destructive_interference_size) std::atomic<size_type> push_reserve_counter{0};
    alignas(hardware_destructive_interference_size) std::atomic<size_type> pop_counter{0};
    alignas(hardware_destructive_interference_size) std::atomic<size_type> pop_reserve_counter{0};
};

template<typename T, std::size_t N>
class mpmc_bounded_queue_v3
{
    using mutex_type = std::mutex;
    using size_type = std::size_t;
    static constexpr size_type capacity_ = N;
    static constexpr size_type(*index)(size_type) = detail::index_<size_type, capacity_+1>;
    static constexpr std::size_t hardware_destructive_interference_size = std::hardware_destructive_interference_size;
    static_assert(std::is_unsigned_v<size_type>);
public:
    using value_type = T;

    ~mpmc_bounded_queue_v3()
    {
        clear();
    }

    template<typename...Args>
    bool try_push(Args&&...args){
        std::unique_lock<mutex_type> lock{push_guard};
        auto push_index_ = push_index.load(std::memory_order::memory_order_relaxed);
        auto next_push_index = index(push_index_+1);
        if (next_push_index == pop_index.load(std::memory_order::memory_order_acquire)){//queue is full
            lock.unlock();
            return false;
        }else{
            elements[push_index_].emplace(std::forward<Args>(args)...);
            push_index.store(next_push_index, std::memory_order::memory_order_release);
            lock.unlock();
            return true;
        }
    }

    bool try_pop(value_type& v){
        std::unique_lock<mutex_type> lock{pop_guard};
        auto pop_index_ = pop_index.load(std::memory_order::memory_order_relaxed);
        if (pop_index_ == push_index.load(std::memory_order::memory_order_acquire)){//queue is empty
            lock.unlock();
            return false;
        }else{
            v = elements[pop_index].get();
            pop_index.store(index(pop_index_+1), std::memory_order::memory_order_release);
            lock.unlock();
            return true;
        }
    }

    template<typename...Args>
    void push(Args&&...args){
        std::unique_lock<mutex_type> lock{push_guard};
        auto push_index_ = push_index.load(std::memory_order::memory_order_relaxed);
        auto next_push_index = index(push_index_+1);
        while(next_push_index == pop_index.load(std::memory_order::memory_order_acquire));//wait until not full
        elements[push_index_].emplace(std::forward<Args>(args)...);
        push_index.store(next_push_index, std::memory_order::memory_order_release);
        lock.unlock();
    }

    void pop(value_type& v){
        std::unique_lock<mutex_type> lock{pop_guard};
        auto pop_index_ = pop_index.load(std::memory_order::memory_order_relaxed);
        while(pop_index_ == push_index.load(std::memory_order::memory_order_acquire));//wait until not empty
        v = elements[pop_index].get();
        pop_index.store(index(pop_index_+1), std::memory_order::memory_order_release);
        lock.unlock();
    }

    auto size()const{
        auto push_index_ = push_index.load(std::memory_order::memory_order_relaxed);
        auto pop_index_ = pop_index.load(std::memory_order::memory_order_relaxed);
        return pop_index_ > push_index_ ? (capacity_+1+push_index_-pop_index_) : (push_index_ - pop_index_);
    }
    constexpr size_type capacity()const{return capacity_;}

private:

    void clear(){
        auto pop_index_ = pop_index.load(std::memory_order::memory_order_relaxed);
        auto push_index_ = push_index.load(std::memory_order::memory_order_relaxed);
        while(pop_index_ != push_index_){
            elements[pop_index].destroy();
            pop_index_ = index(pop_index_+1);
        }
    }

    std::array<detail::element_<value_type>,capacity_+1> elements;
    std::atomic<size_type> push_index;
    std::atomic<size_type> pop_index;
    alignas(hardware_destructive_interference_size) mutex_type push_guard;
    alignas(hardware_destructive_interference_size) mutex_type pop_guard;
};

}   //end of namespace mpmc_bounded_queue

#endif