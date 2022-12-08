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

    // bool try_push(const value_type& v){
    //     if(push_guard.try_lock())
    //     {
    //         auto push_index__ = push_index_.load();
    //         bool is_full{index(push_index__+1)==pop_index_.load()};
    //         if (!is_full){
    //             elements_[push_index__] = v;
    //             push_index_.store(index(push_index__+1));
    //             push_guard.unlock();
    //             return true;
    //         }else{
    //             push_guard.unlock();
    //             return false;
    //         }
    //     }else{
    //         return false;
    //     }
    // }

    // bool try_pop(value_type& v){
    //     if(pop_guard.try_lock())
    //     {
    //         auto pop_index__ = pop_index_.load();
    //         bool is_empty{push_index_.load() == pop_index__};
    //         if (!is_empty){
    //             v = elements_[pop_index_];
    //             pop_index_.store(index(pop_index__+1));
    //             pop_guard.unlock();
    //             return true;
    //         }else{
    //             pop_guard.unlock();
    //             return false;
    //         }
    //     }else{
    //         return false;
    //     }
    // }

    bool try_push(const value_type& v){
        auto lock = std::unique_lock<mutex_type>{push_guard, std::try_to_lock};
        if(lock)
        {
            auto push_index__ = push_index_.load();
            bool is_full{index(push_index__+1)==pop_index_.load()};
            if (!is_full){
                elements_[push_index__] = v;
                push_index_.store(index(push_index__+1));
                return true;
            }else{
                return false;
            }
        }else{
            return false;
        }
    }

    bool try_pop(value_type& v){
        auto lock = std::unique_lock<mutex_type>{pop_guard, std::try_to_lock};
        if(lock)
        {
            auto pop_index__ = pop_index_.load();
            bool is_empty{push_index_.load() == pop_index__};
            if (!is_empty){
                v = elements_[pop_index_];
                pop_index_.store(index(pop_index__+1));
                return true;
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
* counter overflow safe and buffer size may be not pow of 2
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
                    return false;
                }
            }else{
                return false;
            }
        }else{
            return false;
        }
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
    auto index(size_type c)const{return c%(buffer_size);}
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
        for (size_type i{0}; i!=buffer_size; ++i){
            elements_[i].id.store(i);
        }
    }

    std::array<element,buffer_size> elements_{};
    std::atomic<size_type> push_counter_{0};
    std::atomic<size_type> pop_counter_{0};
    std::atomic<bool> debug_stop_{false};
};

template<typename T, std::size_t N, typename SizeT = std::size_t>
class mpmc_lock_free_circular_buffer_v2_cas_loop
{
    static constexpr SizeT size_type_max{std::numeric_limits<SizeT>::max()};
    static constexpr SizeT counter_max{static_cast<SizeT>(size_type_max%N == N-1 ? size_type_max : (size_type_max/N)*N - 1)};
    static_assert(counter_max > N-1);
    static_assert(std::is_unsigned_v<SizeT>);
public:
    using size_type = SizeT;
    using value_type = T;
    static constexpr size_type buffer_size = N;

    mpmc_lock_free_circular_buffer_v2_cas_loop()
    {
        init();
    }

    bool try_push(const value_type& v){
        if (!debug_stop_.load()){
            auto push_counter__ = push_counter_.load();
            while(true){
                auto& element = elements_[index(push_counter__)];
                auto id = element.id.load();
                if (id == push_counter__){ //buffer overwrite protection
                    auto new_push_counter = counter_inc(push_counter__);
                    if (push_counter_.compare_exchange_weak(push_counter__, new_push_counter)){
                        element.value = v;
                        element.id.store(new_push_counter);
                        return true;
                    }
                }else if (id < push_counter__){//queue full, exit
                    return false;
                }else{//element full, try next
                    push_counter__ = push_counter_.load();
                }
            }
        }else{
            return false;
        }
    }

    bool try_pop(value_type& v){
        if (!debug_stop_.load()){
            auto pop_counter__ = pop_counter_.load();
            while(true){
                auto& element = elements_[index(pop_counter__)];
                auto new_pop_counter{counter_inc(pop_counter__)};
                auto id = element.id.load();
                if (id == new_pop_counter){
                    if (pop_counter_.compare_exchange_weak(pop_counter__, new_pop_counter)){//pop_counter__ updated when fails
                        v = element.value;
                        element.id.store(counter_add_buffer_size(pop_counter__));
                        return true;
                    }
                }else if (id < new_pop_counter){//queue empty, exit
                    return false;
                }else{//element empty, try next
                    pop_counter__ = pop_counter_.load();
                }
            }
        }else{
            return false;
        }
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
    auto index(size_type c)const{return c%(buffer_size);}
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
        for (size_type i{0}; i!=buffer_size; ++i){
            elements_[i].id.store(i);
        }
    }

    std::array<element,buffer_size> elements_{};
    std::atomic<size_type> push_counter_{0};
    std::atomic<size_type> pop_counter_{0};
    std::atomic<bool> debug_stop_{false};
};

template<typename T, std::size_t N, typename SizeT = std::size_t>
class mpmc_lock_free_circular_buffer_v2_cas_loop_aligned_counters
{
    static constexpr SizeT size_type_max{std::numeric_limits<SizeT>::max()};
    static constexpr SizeT counter_max{static_cast<SizeT>(size_type_max%N == N-1 ? size_type_max : (size_type_max/N)*N - 1)};
    static_assert(counter_max > N-1);
    static_assert(std::is_unsigned_v<SizeT>);
    static constexpr std::size_t hardware_destructive_interference_size = std::hardware_destructive_interference_size;
public:
    using size_type = SizeT;
    using value_type = T;
    static constexpr size_type buffer_size = N;

    mpmc_lock_free_circular_buffer_v2_cas_loop_aligned_counters()
    {
        static constexpr std::size_t push_counter_offset = offsetof(mpmc_lock_free_circular_buffer_v2_cas_loop_aligned_counters, push_counter_);
        static constexpr std::size_t pop_counter_offset = offsetof(mpmc_lock_free_circular_buffer_v2_cas_loop_aligned_counters, pop_counter_);
        static_assert(std::max(push_counter_offset,pop_counter_offset) - std::min(push_counter_offset,pop_counter_offset) >= hardware_destructive_interference_size);
        init();
    }

    bool try_push(const value_type& v){
        if (!debug_stop_.load()){
            auto push_counter__ = push_counter_.load();
            while(true){
                auto& element = elements_[index(push_counter__)];
                auto id = element.id.load();
                if (id == push_counter__){ //buffer overwrite protection
                    auto new_push_counter = counter_inc(push_counter__);
                    if (push_counter_.compare_exchange_weak(push_counter__, new_push_counter)){
                        element.value = v;
                        element.id.store(new_push_counter);
                        return true;
                    }
                }else if (id < push_counter__){//queue full, exit
                    return false;
                }else{//element full, try next
                    push_counter__ = push_counter_.load();
                }
            }
        }else{
            return false;
        }
    }

    bool try_pop(value_type& v){
        if (!debug_stop_.load()){
            auto pop_counter__ = pop_counter_.load();
            while(true){
                auto& element = elements_[index(pop_counter__)];
                auto new_pop_counter{counter_inc(pop_counter__)};
                auto id = element.id.load();
                if (id == new_pop_counter){
                    if (pop_counter_.compare_exchange_weak(pop_counter__, new_pop_counter)){//pop_counter__ updated when fails
                        v = element.value;
                        element.id.store(counter_add_buffer_size(pop_counter__));
                        return true;
                    }
                }else if (id < new_pop_counter){//queue empty, exit
                    return false;
                }else{//element empty, try next
                    pop_counter__ = pop_counter_.load();
                }
            }
        }else{
            return false;
        }
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
    auto index(size_type c)const{return c%(buffer_size);}
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
        for (size_type i{0}; i!=buffer_size; ++i){
            elements_[i].id.store(i);
        }
    }

    std::array<element,buffer_size> elements_{};
    alignas(hardware_destructive_interference_size) std::atomic<size_type> push_counter_{0};
    alignas(hardware_destructive_interference_size) std::atomic<size_type> pop_counter_{0};
    std::atomic<bool> debug_stop_{false};
};

template<typename T, std::size_t N, typename SizeT = std::size_t>
class mpmc_lock_free_circular_buffer_v2_cas_loop_aligned_counters_elements
{
    static constexpr SizeT size_type_max{std::numeric_limits<SizeT>::max()};
    static constexpr SizeT counter_max{static_cast<SizeT>(size_type_max%N == N-1 ? size_type_max : (size_type_max/N)*N - 1)};
    static_assert(counter_max > N-1);
    static_assert(std::is_unsigned_v<SizeT>);
    static constexpr std::size_t hardware_destructive_interference_size = std::hardware_destructive_interference_size;
public:
    using size_type = SizeT;
    using value_type = T;
    static constexpr size_type buffer_size = N;

    mpmc_lock_free_circular_buffer_v2_cas_loop_aligned_counters_elements()
    {
        static constexpr std::size_t push_counter_offset = offsetof(mpmc_lock_free_circular_buffer_v2_cas_loop_aligned_counters_elements, push_counter_);
        static constexpr std::size_t pop_counter_offset = offsetof(mpmc_lock_free_circular_buffer_v2_cas_loop_aligned_counters_elements, pop_counter_);
        static_assert(std::max(push_counter_offset,pop_counter_offset) - std::min(push_counter_offset,pop_counter_offset) >= hardware_destructive_interference_size);
        static_assert(alignof(element) == hardware_destructive_interference_size);
        init();
    }

    bool try_push(const value_type& v){
        if (!debug_stop_.load()){
            auto push_counter__ = push_counter_.load();
            while(true){
                auto& element = elements_[index(push_counter__)];
                auto id = element.id.load();
                if (id == push_counter__){ //buffer overwrite protection
                    auto new_push_counter = counter_inc(push_counter__);
                    if (push_counter_.compare_exchange_weak(push_counter__, new_push_counter)){
                        element.value = v;
                        element.id.store(new_push_counter);
                        return true;
                    }
                }else if (id < push_counter__){//queue full, exit
                    return false;
                }else{//element full, try next
                    push_counter__ = push_counter_.load();
                }
            }
        }else{
            return false;
        }
    }

    bool try_pop(value_type& v){
        if (!debug_stop_.load()){
            auto pop_counter__ = pop_counter_.load();
            while(true){
                auto& element = elements_[index(pop_counter__)];
                auto new_pop_counter{counter_inc(pop_counter__)};
                auto id = element.id.load();
                if (id == new_pop_counter){
                    if (pop_counter_.compare_exchange_weak(pop_counter__, new_pop_counter)){//pop_counter__ updated when fails
                        v = element.value;
                        element.id.store(counter_add_buffer_size(pop_counter__));
                        return true;
                    }
                }else if (id < new_pop_counter){//queue empty, exit
                    return false;
                }else{//element empty, try next
                    pop_counter__ = pop_counter_.load();
                }
            }
        }else{
            return false;
        }
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
    auto index(size_type c)const{return c%(buffer_size);}
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

    struct alignas(hardware_destructive_interference_size) element
    {
        value_type value{};
        std::atomic<size_type> id{};
    };

    void init(){
        for (size_type i{0}; i!=buffer_size; ++i){
            elements_[i].id.store(i);
        }
    }

    std::array<element,buffer_size> elements_{};
    alignas(hardware_destructive_interference_size) std::atomic<size_type> push_counter_{0};
    alignas(hardware_destructive_interference_size) std::atomic<size_type> pop_counter_{0};
    std::atomic<bool> debug_stop_{false};
};

template<typename T, std::size_t N, typename SizeT = std::size_t>
class mpmc_lock_free_circular_buffer_v3
{
    static constexpr SizeT size_type_max{std::numeric_limits<SizeT>::max()};
    static constexpr SizeT counter_max{static_cast<SizeT>(size_type_max%(N+1) == N ? size_type_max : (size_type_max/(N+1))*(N+1) - 1)};
    static_assert(counter_max > N);
    static_assert(std::is_unsigned_v<SizeT>);
public:
    using size_type = SizeT;
    using value_type = T;
    static constexpr size_type buffer_size = N;

    mpmc_lock_free_circular_buffer_v3()
    {}

    bool try_push(const value_type& v){
        if (!debug_stop_.load()){
            auto push_reserve_counter__ = push_reserve_counter_.load();
            auto next_push_reserve_counter = inc_counter(push_reserve_counter__);
            if (index(pop_counter_.load()) == index(next_push_reserve_counter)){  //full
                return false;
            }else{
                if (push_reserve_counter_.compare_exchange_weak(push_reserve_counter__, next_push_reserve_counter)){
                    elements_[index(push_reserve_counter__)] = v;
                    while(push_counter_.load() != push_reserve_counter__){};//wait for prev pushes
                    push_counter_.store(next_push_reserve_counter);
                    return true;
                }else{
                    return false;
                }
            }
        }else{
            return false;
        }
    }

    bool try_pop(value_type& v){
        if (!debug_stop_.load()){
            auto pop_reserve_counter__ = pop_reserve_counter_.load();
            auto next_pop_reserve_counter = inc_counter(pop_reserve_counter__);
            if (push_counter_.load() == pop_reserve_counter__){   //empty
                return false;
            }else{
                if (pop_reserve_counter_.compare_exchange_weak(pop_reserve_counter__, next_pop_reserve_counter)){
                    v = elements_[index(pop_reserve_counter__)];
                    while(pop_counter_.load() != pop_reserve_counter__){};//wait for prev pops
                    pop_counter_.store(next_pop_reserve_counter);
                    return true;
                }else{
                    return false;
                }
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
        res<<std::endl<<static_cast<std::size_t>(push_counter_.load())<<" "<<static_cast<std::size_t>(push_reserve_counter_.load())<<" "<<
            static_cast<std::size_t>(pop_counter_.load())<<" "<<static_cast<std::size_t>(pop_reserve_counter_.load())<<std::endl;
        for (const auto& i : elements_){
            res<<i<<",";
        }
        return res.str();
    }

private:

    auto index(size_type c)const{return c%(buffer_size+1);}
    size_type(*inc_counter)(size_type) = inc_counter_<counter_max>;
    template<size_type CounterMax>
    static auto inc_counter_(size_type c){return static_cast<size_type>(c==CounterMax ? 0 : c+1);}
    template<>
    static auto inc_counter_<size_type_max>(size_type c){return static_cast<size_type>(c+1);}

    std::array<value_type, buffer_size+1> elements_{};
    std::atomic<size_type> push_counter_{0};
    std::atomic<size_type> push_reserve_counter_{0};
    std::atomic<size_type> pop_counter_{0};
    std::atomic<size_type> pop_reserve_counter_{0};
    std::atomic<bool> debug_stop_{false};
};



}   //end of namespace experimental_multithreading


#endif