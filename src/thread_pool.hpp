#ifndef THREAD_POOL_HPP_
#define THREAD_POOL_HPP_
#include <tuple>

template<std::size_t N_Threads, typename> class thread_pool;

//specialization for functor of specific type
//R, Args... extracts from functor's operator() signature

//specialization for std::function<R(Args...)>

//specialization for function pointer
template<std::size_t N_Threads, typename R, typename...Args>
class thread_pool<N_Threads, R(Args...)>
{

};

#endif