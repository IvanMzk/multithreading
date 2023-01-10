#ifndef MPMC_BOUNDED_QUEUE_HPP_
#define MPMC_BOUNDED_QUEUE_HPP_

#include <atomic>
#include <mutex>
#include <array>
#include <exception>

namespace mpmc_bounded_queue{

namespace detail{

template<typename SizeT>
inline auto index_(SizeT cnt, SizeT cap){return cnt%cap;}

static constexpr std::size_t hardware_destructive_interference_size = std::hardware_destructive_interference_size;

template<typename T>
class element_
{
    using value_type = T;
public:
    //left buffer uninitialized
    element_(){}

    template<typename...Args>
    void emplace(Args&&...args){
        new(reinterpret_cast<void*>(buffer)) value_type{std::forward<Args>(args)...};
    }
    value_type&& move(){
        return std::move(*reinterpret_cast<value_type*>(buffer));
    }
    template<typename V>
    void move(V& v){
        v = std::move(*reinterpret_cast<value_type*>(buffer));
    }
    const value_type& get()const{
        return *reinterpret_cast<value_type*>(buffer);
    }
    value_type& get(){
        return *reinterpret_cast<value_type*>(buffer);
    }
    void destroy(){
        reinterpret_cast<value_type*>(buffer)->~value_type();
    }
private:
    alignas(value_type) std::byte buffer[sizeof(value_type)];
};

template<typename T>
class element_v1_ : public element_<T>
{
    public:
        using size_type = std::size_t;
        alignas(hardware_destructive_interference_size) std::atomic<size_type> id{};
};

template<typename T>
class element
{
public:
    using value_type = T;
    ~element()
    {
        clear();
    }
    element(){}
    element(const element& other)
    {
        init(other);
    }
    element(element&& other)
    {
        init(std::move(other));
    }
    element& operator=(const element& other){
        clear();
        init(other);
        return *this;
    }
    element& operator=(element&& other){
        clear();
        init(std::move(other));
        return *this;
    }
    element& operator=(value_type&& v){
        element__.emplace(std::move(v));
        empty_ = false;
        return *this;
    }
    operator bool()const{return !empty_;}
    auto empty()const{return empty_;}
    auto& get()const{return element__.get();}
    auto& get(){return element__.get();}
private:
    void clear(){
        if (!empty_){
            element__.destroy();
        }
    }
    void init(const element& other){
        if (!other.empty_){
            element__.emplace(other.element__.get());
            empty_ = false;
        }
    }
    void init(element&& other){
        if (!other.empty_){
            element__.emplace(other.element__.move());
            empty_ = false;
        }
    }

    element_<value_type> element__{};
    bool empty_{true};
};

}   //end of namespace detail

//multiple producer multiple consumer bounded queue
template<typename T, typename Allocator = std::allocator<detail::element_v1_<T>>>
class mpmc_bounded_queue_v1
{
    using element_type = typename std::allocator_traits<Allocator>::value_type;
    using size_type = typename element_type::size_type;
    static_assert(std::is_unsigned_v<size_type>);
public:
    using value_type = T;
    using allocator_type = Allocator;

    mpmc_bounded_queue_v1(size_type capacity__, const Allocator& allocator__ = Allocator{}):
        capacity_{capacity__},
        allocator{allocator__}
    {
        if (capacity_ <= 1){
            throw std::invalid_argument("queue capacity must be > 1");
        }
        elements = allocator.allocate(capacity_);
        init();
    }
    ~mpmc_bounded_queue_v1()
    {
        clear();
        allocator.deallocate(elements, capacity_);
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
        return try_pop_(v);
    }
    auto try_pop(){
        detail::element<value_type> v{};
        try_pop_(v);
        return v;
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
        pop_(v);
    }
    auto pop(){
        detail::element<value_type> v{};
        pop_(v);
        return v;
    }

    auto size()const{return push_counter.load(std::memory_order::memory_order_relaxed) - pop_counter.load(std::memory_order::memory_order_relaxed);}
    auto capacity()const{return capacity_;}

private:

    template<typename V>
    bool try_pop_(V& v){
        auto pop_counter_ = pop_counter.load(std::memory_order::memory_order_relaxed);
        while(true){
            auto& element = elements[index(pop_counter_)];
            auto next_pop_counter = pop_counter_+1;
            auto id = element.id.load(std::memory_order::memory_order_acquire);
            if (id == next_pop_counter){
                if (pop_counter.compare_exchange_weak(pop_counter_, next_pop_counter, std::memory_order::memory_order_relaxed)){//pop_counter_ updated when fails
                    element.move(v);
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

    template<typename V>
    void pop_(V& v){
        auto pop_counter_ = pop_counter.fetch_add(1, std::memory_order::memory_order_relaxed);    //reserve
        auto next_pop_counter = pop_counter_+1;
        auto& element = elements[index(pop_counter_)];
        while(next_pop_counter != element.id.load(std::memory_order::memory_order_acquire)){};  //wait until element is full
        element.move(v);
        element.id.store(pop_counter_+capacity_, std::memory_order::memory_order_release);
    }

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

    void init(){
        for (size_type i{0}; i!=capacity_; ++i){
            elements[i].id.store(i);
        }
    }

    auto index(size_type cnt){return detail::index_(cnt, capacity_);}

    size_type capacity_;
    allocator_type allocator;
    element_type* elements;
    alignas(detail::hardware_destructive_interference_size) std::atomic<size_type> push_counter{0};
    alignas(detail::hardware_destructive_interference_size) std::atomic<size_type> pop_counter{0};
};

template<typename T, typename Allocator = std::allocator<detail::element_<T>>>
class mpmc_bounded_queue_v2
{
    using element_type = typename std::allocator_traits<Allocator>::value_type;
    using size_type = std::size_t;
    static_assert(std::is_unsigned_v<size_type>);
public:
    using value_type = T;
    using allocator_type = Allocator;

    mpmc_bounded_queue_v2(size_type capacity__, const allocator_type& alloc = allocator_type()):
        capacity_{capacity__},
        allocator{alloc}
    {
        if (capacity_ == 0){
            throw std::invalid_argument("queue capacity must be > 0");
        }
        elements = allocator.allocate(capacity_+1);
    }
    ~mpmc_bounded_queue_v2()
    {
        clear();
        allocator.deallocate(elements, capacity_+1);
    }

    template<typename...Args>
    bool try_push(Args&&...args){
        auto push_reserve_counter_ = push_reserve_counter.load(std::memory_order::memory_order_relaxed);
        while(true){
            if (push_reserve_counter_ - pop_counter.load(std::memory_order::memory_order_acquire) >= capacity_){  //full    //acquaire1
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
        return try_pop_(v);
    }
    auto try_pop(){
        detail::element<value_type> v{};
        try_pop_(v);
        return v;
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
        pop_(v);
    }
    auto pop(){
        detail::element<value_type> v{};
        pop_(v);
        return v;
    }

    auto size()const{return push_counter.load(std::memory_order::memory_order_relaxed) - pop_counter.load(std::memory_order::memory_order_relaxed);}
    auto capacity()const{return capacity_;}

private:

    template<typename V>
    bool try_pop_(V& v){
        auto pop_reserve_counter_ = pop_reserve_counter.load(std::memory_order::memory_order_relaxed);
        while(true){
            if (pop_reserve_counter_ >= push_counter.load(std::memory_order::memory_order_acquire)){   //empty   //acquaire0
                return false;
            }else{
                auto next_pop_reserve_counter = pop_reserve_counter_+1;
                if (pop_reserve_counter.compare_exchange_weak(pop_reserve_counter_, next_pop_reserve_counter, std::memory_order::memory_order_relaxed)){
                    elements[index(pop_reserve_counter_)].move(v);
                    while(pop_counter.load(std::memory_order::memory_order_acquire) != pop_reserve_counter_){}//wait for prev pops  //acquaire1
                    pop_counter.store(next_pop_reserve_counter, std::memory_order::memory_order_release);   //release1
                    return true;
                }
            }
        }
    }

    template<typename V>
    void pop_(V& v){
        auto pop_reserve_counter_ = pop_reserve_counter.fetch_add(1, std::memory_order::memory_order_relaxed);
        while(pop_reserve_counter_ >= push_counter.load(std::memory_order::memory_order_acquire)){}   //wait until not empty
        elements[index(pop_reserve_counter_)].move(v);
        while(pop_counter.load(std::memory_order::memory_order_acquire) != pop_reserve_counter_){}   //wait for prev pops
        pop_counter.store(pop_reserve_counter_+1, std::memory_order::memory_order_release);    //commit
    }

    void clear(){
        auto pop_reserve_counter_ = pop_reserve_counter.load(std::memory_order::memory_order_relaxed);
        auto push_counter_ = push_counter.load(std::memory_order::memory_order_relaxed);
        while(push_counter_ != pop_reserve_counter_){
            elements[index(pop_reserve_counter_)].destroy();
            ++pop_reserve_counter_;
        }
    }

    auto index(size_type cnt){return detail::index_(cnt, capacity_+1);}

    size_type capacity_;
    allocator_type allocator;
    element_type* elements;
    alignas(detail::hardware_destructive_interference_size) std::atomic<size_type> push_counter{0};
    alignas(detail::hardware_destructive_interference_size) std::atomic<size_type> push_reserve_counter{0};
    alignas(detail::hardware_destructive_interference_size) std::atomic<size_type> pop_counter{0};
    alignas(detail::hardware_destructive_interference_size) std::atomic<size_type> pop_reserve_counter{0};
};

template<typename T, typename Allocator = std::allocator<detail::element_<T>>>
class mpmc_bounded_queue_v3
{
    using element_type = typename std::allocator_traits<Allocator>::value_type;
    using size_type = std::size_t;
    using mutex_type = std::mutex;
    static_assert(std::is_unsigned_v<size_type>);
public:
    using value_type = T;
    using allocator_type = Allocator;

    mpmc_bounded_queue_v3(size_type capacity__, const allocator_type& alloc = allocator_type()):
        capacity_{capacity__},
        allocator{alloc}
    {
        if (capacity_ == 0){
            throw std::invalid_argument("queue capacity must be > 0");
        }
        elements = allocator.allocate(capacity_+1);
    }
    ~mpmc_bounded_queue_v3()
    {
        clear();
        allocator.deallocate(elements, capacity_+1);
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
        return try_pop_(v);
    }
    auto try_pop(){
        detail::element<value_type> v{};
        try_pop_(v);
        return v;
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
        pop_(v);
    }
    auto pop(){
        detail::element<value_type> v{};
        pop_(v);
        return v;
    }

    auto size()const{
        auto push_index_ = push_index.load(std::memory_order::memory_order_relaxed);
        auto pop_index_ = pop_index.load(std::memory_order::memory_order_relaxed);
        return pop_index_ > push_index_ ? (capacity_+1+push_index_-pop_index_) : (push_index_ - pop_index_);
    }
    auto capacity()const{return capacity_;}

private:

    template<typename V>
    bool try_pop_(V& v){
        std::unique_lock<mutex_type> lock{pop_guard};
        auto pop_index_ = pop_index.load(std::memory_order::memory_order_relaxed);
        if (pop_index_ == push_index.load(std::memory_order::memory_order_acquire)){//queue is empty
            lock.unlock();
            return false;
        }else{
            elements[pop_index].move(v);
            pop_index.store(index(pop_index_+1), std::memory_order::memory_order_release);
            lock.unlock();
            return true;
        }
    }

    template<typename V>
    void pop_(V& v){
        std::unique_lock<mutex_type> lock{pop_guard};
        auto pop_index_ = pop_index.load(std::memory_order::memory_order_relaxed);
        while(pop_index_ == push_index.load(std::memory_order::memory_order_acquire));//wait until not empty
        elements[pop_index].move(v);
        pop_index.store(index(pop_index_+1), std::memory_order::memory_order_release);
        lock.unlock();
    }

    void clear(){
        auto pop_index_ = pop_index.load(std::memory_order::memory_order_relaxed);
        auto push_index_ = push_index.load(std::memory_order::memory_order_relaxed);
        while(pop_index_ != push_index_){
            elements[pop_index].destroy();
            pop_index_ = index(pop_index_+1);
        }
    }

    auto index(size_type cnt){return detail::index_(cnt, capacity_+1);}

    size_type capacity_;
    allocator_type allocator;
    element_type* elements;
    std::atomic<size_type> push_index;
    std::atomic<size_type> pop_index;
    alignas(detail::hardware_destructive_interference_size) mutex_type push_guard;
    alignas(detail::hardware_destructive_interference_size) mutex_type pop_guard;
};

//single thread bounded queue
template<typename T, typename Allocator = std::allocator<detail::element_<T>>>
class st_bounded_queue
{
    using element_type = typename std::allocator_traits<Allocator>::value_type;
    using size_type = std::size_t;
    static_assert(std::is_unsigned_v<size_type>);
public:
    using value_type = T;
    using allocator_type = Allocator;

    st_bounded_queue(size_type capacity__, const allocator_type& alloc = allocator_type()):
        capacity_{capacity__},
        allocator{alloc}
    {
        if (capacity_ == 0){
            throw std::invalid_argument("queue capacity must be > 0");
        }
        elements = allocator.allocate(capacity_+1);
    }
    ~st_bounded_queue()
    {
        clear();
        allocator.deallocate(elements, capacity_+1);
    }

    template<typename...Args>
    auto try_push(Args&&...args){
        value_type* res{nullptr};
        auto next_push_index = index(push_index+1);
        if (next_push_index==pop_index){
            return res;
        }else{
            elements[push_index].emplace(std::forward<Args>(args)...);
            res = &elements[push_index].get();
            push_index = next_push_index;
            return res;
        }
    }

    bool try_pop(value_type& v){
        return try_pop_(v);
    }
    auto try_pop(){
        detail::element<value_type> v{};
        try_pop_(v);
        return v;
    }

    auto size()const{return pop_index > push_index ? (capacity_+1+push_index-pop_index) : (push_index - pop_index);}
    auto capacity()const{return capacity_;}

private:

    template<typename V>
    bool try_pop_(V& v){
        if (push_index == pop_index){
            return false;
        }else{
            elements[pop_index].move(v);
            pop_index = index(pop_index+1);
            return true;
        }
    }

    void clear(){
        while(pop_index != push_index){
            elements[pop_index].destroy();
            pop_index = index(pop_index+1);
        }
    }

    auto index(size_type cnt){return detail::index_(cnt, capacity_+1);}

    size_type capacity_;
    allocator_type allocator;
    element_type* elements;
    size_type push_index{0};
    size_type pop_index{0};
};


}   //end of namespace mpmc_bounded_queue

#endif