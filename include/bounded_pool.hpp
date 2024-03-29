/*
* Copyright (c) 2022 Ivan Malezhyk <ivanmzk@gmail.com>
*
* Distributed under the Boost Software License, Version 1.0.
* The full license is in the file LICENSE.txt, distributed with this software.
*/

#ifndef BOUNDED_POOL_HPP_
#define BOUNDED_POOL_HPP_

#include "queue.hpp"

namespace bounded_pool{

namespace detail{

class queue_of_refs
{
    using queue_type = queue::mpmc_bounded_queue_v3<void*>;
    queue_type refs;
public:
    queue_of_refs(std::size_t capacity_):
        refs{capacity_}
    {}
    auto size()const{return refs.size();}
    auto capacity()const{return refs.capacity();}
    void push(void* ref){refs.push(ref);}
    auto pop(){return refs.pop();}
    auto try_pop(){return refs.try_pop();}
};

template<typename T>
class shareable_element{
    using value_type = T;
    using pool_type = queue_of_refs;
public:
    template<typename...Args>
    shareable_element(pool_type* pool_, Args&&...args):
        pool{pool_},
        value{std::forward<Args>(args)...}
    {}
    auto make_shared(){
        inc_ref();
        return shared_element{this};
    }
    static auto make_empty_shared(){
        return shared_element{nullptr};
    }
private:
    pool_type* pool;
    value_type value;
    std::atomic<std::size_t> use_count_{0};

    class shared_element{
        friend class shareable_element;
        shareable_element* elem;
        shared_element(shareable_element* elem_):
            elem{elem_}
        {}
        void inc_ref(){
            if (elem){
                elem->inc_ref();
            }
        }
        void dec_ref(){
            if (elem){
                elem->dec_ref();
            }
        }
    public:
        ~shared_element()
        {
            dec_ref();
        }
        shared_element() = default;
        shared_element(const shared_element& other):
            elem{other.elem}
        {
            inc_ref();
        }
        shared_element(shared_element&& other):
            elem{other.elem}
        {
            other.elem = nullptr;
        }
        shared_element& operator=(const shared_element& other){
            dec_ref();
            elem = other.elem;
            inc_ref();
            return *this;
        }
        shared_element& operator=(shared_element&& other){
            dec_ref();
            elem = other.elem;
            other.elem = nullptr;
            return *this;
        }
        operator bool()const{return static_cast<bool>(elem);}
        void reset(){
            dec_ref();
            elem = nullptr;
        }
        auto use_count()const{return elem->use_count();}
        auto& get(){return elem->get();}
        auto& get()const{return elem->get();}
    };

    auto inc_ref(){return use_count_.fetch_add(1);}
    auto dec_ref(){
        if (use_count_.fetch_sub(1) == 1){
            pool->push(this);
        }
    }
    auto use_count()const{return use_count_.load();}
    auto& get(){return value;}
    auto& get()const{return value;}
};

}   //end of namespace detail

//multiple consumer bounded pool of reusable objects
template<typename T, typename Allocator = std::allocator<detail::shareable_element<T>>>
class mc_bounded_pool
{

    using element_type = typename std::allocator_traits<Allocator>::value_type;
    using pool_type = detail::queue_of_refs;

public:
    using value_type = T;
    using allocator_type = Allocator;

    template<typename...Args>
    explicit mc_bounded_pool(std::size_t capacity__, Args&&...args):
        allocator{allocator_type{}},
        pool(capacity__),
        elements{allocator.allocate(capacity__)}
    {
        init(std::forward<Args>(args)...);
    }

    template<typename It, std::enable_if_t<!std::is_convertible_v<It,std::size_t>,int> = 0>
    mc_bounded_pool(It first, It last, const Allocator& allocator__ = Allocator{}):
        allocator{allocator__},
        pool(std::distance(first,last)),
        elements{allocator.allocate(pool.capacity())}
    {
        init(first, last);
    }

    ~mc_bounded_pool()
    {
        clear();
        allocator.deallocate(elements,capacity());
    }

    //returns shared_element object that is smart wrapper of reference to reusable object
    //after last copy of shared_element object is destroyed reusable object is returned to pool and ready to new use
    //blocks until reusable object is available
    auto pop(){
        return static_cast<element_type*>(pool.pop().get())->make_shared();
    }

    //not blocking, if no reusable objects available returns immediately
    //result, that is shared_element object, can be converted to bool and test, false if no objects available, true otherwise
    auto try_pop(){
        if (auto e = pool.try_pop()){
            return static_cast<element_type*>(e.get())->make_shared();
        }else{
            return element_type::make_empty_shared();
        }
    }

    auto size()const{return pool.size();}
    auto capacity()const{return pool.capacity();}
    auto empty()const{return size() == 0;}
private:
    template<typename...Args>
    void init(Args&&...args){
        auto it = elements;
        auto end = elements+capacity();
        for(;it!=end;++it){new(it) element_type{&pool, std::forward<Args>(args)...};}
        init_pool();
    }

    template<typename It>
    void init(It first, It last){
        auto it = elements;
        for(;first!=last;++it,++first){new(it) element_type{&pool, *first};}
        init_pool();
    }
    void init_pool(){std::for_each(elements,elements+capacity(),[this](auto& e){pool.push(&e);});}
    void clear(){std::destroy(elements,elements+capacity());}

    allocator_type allocator;
    pool_type pool;
    element_type* elements;
};


}   //end of namespace bounded_pool

#endif