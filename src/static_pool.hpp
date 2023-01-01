#ifndef STATIC_POOL_HPP_
#define STATIC_POOL_HPP_
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <array>



template<typename T, std::size_t N_Elements, typename MutexT = std::mutex>
class static_pool
{
public:

    using mutex_type = MutexT;
    using value_type = T;

    static_pool():
        static_pool(std::make_index_sequence<N_Elements>{})
    {}
    template<typename...Args>
    static_pool(Args&&...args):
        static_pool(std::make_index_sequence<N_Elements>{}, std::forward<Args>(args)...)
    {}
    //pops element from pool
    //wait = false: immidiately return empty element if pool is empty
    //wait = true: wait until pool is not empty, push signals when element is returned to pool
    auto pop(bool wait = false){
        std::unique_lock<mutex_type> lock{guard};
        if (end_){
            return pool_[--end_]->make_shared_element();
        }else{
            if (wait){
                is_empty.wait(lock,[this](){return !empty();});
                return pool_[--end_]->make_shared_element();
            }else{
                return element::make_empty_shared_element();
            }
        }

    }
    auto size()const{
        return end_;
    }
    auto empty()const{
        return !static_cast<bool>(size());
    }
private:

    template<std::size_t...I, typename...Args>
    static_pool(std::index_sequence<I...>, Args&&...args):
        elements_{element::make_element(this, I, std::forward<Args>(args)...)...},
        pool_{&elements_[I]...}
    {}

    class element{
        static_pool* pool_;
        value_type impl_;
        std::atomic<std::size_t> use_count_{0};
        template<typename...Args>
        element(static_pool* pool__, Args&&...args):
            pool_{pool__},
            impl_{std::forward<Args>(args)...}
        {}
        auto get()const{return &impl_;}
        void inc_ref(){use_count_.fetch_add(1);}
        void dec_ref(){
            if (use_count_.fetch_sub(1) == 1){
                pool_->push(this);
            }
        }

        class shared_element{
            friend class element;
            element* elem_;
            explicit shared_element(element* elem__):
                elem_{elem__}
            {}
        public:
            ~shared_element(){
                if(elem_){
                    elem_->dec_ref();
                }
            }
            shared_element(const shared_element& other):
                elem_{other.elem_}
            {
                elem_->inc_ref();
            }
            operator bool(){return static_cast<bool>(elem_);}
            void reset(){
                if (elem_){
                    elem_->dec_ref();
                    elem_ = nullptr;
                }
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
        static auto make_empty_shared_element(){return shared_element{nullptr};}
        template<typename...Args>
        static auto make_element(static_pool* pool__, std::size_t, Args&&...args){
            return element{pool__, std::forward<Args>(args)...};
        }
    };

    auto push(element* e){
        std::unique_lock<mutex_type> lock{guard};
        pool_[end_] = e;
        ++end_;
        is_empty.notify_one();
    }

    std::array<element,N_Elements> elements_;
    std::array<element*,N_Elements> pool_;
    std::size_t end_{N_Elements};
    mutex_type guard{};
    std::condition_variable is_empty{};
};

}   //end of namespace experimental_multithreading

#endif