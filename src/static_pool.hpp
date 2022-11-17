#ifndef STATIC_POOL_HPP_
#define STATIC_POOL_HPP_
#include <atomic>
#include <mutex>
#include <array>

namespace experimental_multithreading{

template<typename T, std::size_t N_Elements, typename MutexT = std::mutex>
class static_pool
{
public:

    using mutex_type = MutexT;
    using value_type = T;

    static_pool():
        static_pool(std::make_index_sequence<N_Elements>{})
    {}

    auto pop(){
        std::unique_lock<mutex_type> lock{guard};
        --end_;
        return pool_[end_]->make_shared_element();
    }
    auto size()const{
        return end_;
    }
private:

    template<std::size_t...I>
    static_pool(std::index_sequence<I...>):
        elements_{element::make_element(this, I)...},
        pool_{&elements_[I]...}
    {}

    class element{
        static_pool* pool_;
        std::unique_ptr<value_type> impl_;
        std::atomic<std::size_t> use_count_{0};
        element(static_pool* pool__):
            pool_{pool__},
            impl_{std::make_unique<value_type>()}
        {}
        auto inc_ref(){return use_count_.fetch_add(1);}
        auto dec_ref(){return use_count_.fetch_sub(1);}
        auto get()const{return impl_.get();}
        auto push_to_pool(){pool_->push(this);}

        class shared_element{
            friend class element;
            element* elem_;
            shared_element(element* elem__):
                elem_{elem__}
            {}
        public:
            ~shared_element(){
                if (elem_->dec_ref() == 1){
                    elem_->push_to_pool();
                }
            }
            shared_element(const shared_element& other):
                elem_{other.elem_}
            {
                elem_->inc_ref();
            }
            auto use_count()const{return elem_->use_count();}
            auto get(){return elem_->get();}
        };

    public:
        auto use_count()const{return use_count_.load();}
        auto make_shared_element(){
            inc_ref();
            return shared_element{this};
        }
        static auto make_element(static_pool* pool__, std::size_t){
            return element{pool__};
        }
    };

    auto push(element* e){
        std::unique_lock<mutex_type> lock{guard};
        pool_[end_] = e;
        ++end_;
    }

    std::array<element,N_Elements> elements_;
    std::array<element*,N_Elements> pool_;
    std::size_t end_{N_Elements};
    mutex_type guard{};
};

}   //end of namespace experimental_multithreading

#endif