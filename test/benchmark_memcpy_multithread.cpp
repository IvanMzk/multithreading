#include <numeric>
#include "catch.hpp"
#include "thread_pool.hpp"
#include "benchmark_helpers.hpp"

namespace benchmark_memcpy_multithread{

static constexpr std::size_t n_memcpy_workers = 10;
static experimental_multithreading::thread_pool<n_memcpy_workers,n_memcpy_workers, decltype(std::memcpy)> memcpy_pool{};

template<std::size_t> auto memcpy_multithread(void* dst, const void* src, std::size_t n);

template<>
auto memcpy_multithread<1>(void* dst, const void* src, std::size_t n){
    std::memcpy(dst,src,n);
}

// template<std::size_t N>
// auto memcpy_multithread(void* dst, const void* src, std::size_t n){
//     static_assert(N>1);
//     if (n!=0){
//         std::array<decltype(memcpy_pool)::future_type, N> futures{};
//         auto n_chunk = n/N;
//         auto n_first_chunk = n_chunk + n%N;
//         futures[0] = memcpy_pool.push(std::memcpy, dst,src,n_first_chunk);
//         if (n_chunk){
//             auto dst_ = reinterpret_cast<unsigned char*>(dst);
//             auto src_ = reinterpret_cast<const unsigned char*>(src);
//             dst_+=n_first_chunk;
//             src_+=n_first_chunk;
//             for (std::size_t i{1}; i!=N; ++i,dst_+=n_chunk,src_+=n_chunk){
//                 futures[i] = memcpy_pool.push(std::memcpy, dst_,src_,n_chunk);
//             }
//         }
//     }
// }

template<std::size_t N>
auto memcpy_multithread(void* dst, const void* src, std::size_t n){
    static_assert(N>1);
    if (n!=0){
        std::array<decltype(memcpy_pool)::future_type, N-1> futures{};
        auto n_chunk = n/N;
        auto n_last_chunk = n_chunk + n%N;
        auto dst_ = reinterpret_cast<unsigned char*>(dst);
        auto src_ = reinterpret_cast<const unsigned char*>(src);
        if (n_chunk){
            for (std::size_t i{0}; i!=N-1; ++i,dst_+=n_chunk,src_+=n_chunk){
                futures[i] = memcpy_pool.push(std::memcpy, dst_,src_,n_chunk);
            }
        }
        std::memcpy(dst_,src_,n_last_chunk);
    }
}

}   //end of namespace benchmark_memcpy_multithread


TEMPLATE_TEST_CASE("benchmark_memcpy_multithread","[benchmark_memcpy_multithread]",
    (std::integral_constant<std::size_t,1>),
    (std::integral_constant<std::size_t,2>),
    (std::integral_constant<std::size_t,3>),
    (std::integral_constant<std::size_t,4>),
    (std::integral_constant<std::size_t,5>),
    (std::integral_constant<std::size_t,6>),
    (std::integral_constant<std::size_t,7>),
    (std::integral_constant<std::size_t,8>)
)
{
    using threads_number_type = TestType;
    using value_type = float;
    using benchmark_memcpy_multithread::memcpy_multithread;
    using benchmark_helpers::cpu_timer;
    using benchmark_helpers::make_sizes;
    using benchmark_helpers::size_to_str;
    using benchmark_helpers::bandwidth_to_str;



    static_assert(std::is_trivially_copyable_v<value_type>);
    constexpr std::size_t initial_size{1<<20};
    constexpr std::size_t factor{2};
    constexpr std::size_t n{10};
    constexpr auto sizes = make_sizes<initial_size,factor,n>();
    //constexpr std::array<size_t, 10Ui64> sizes = {{1048576Ui64, 2097152Ui64, 4194304Ui64, 8388608Ui64, 16777216Ui64, 33554432Ui64, 67108864Ui64, 134217728Ui64, 268435456Ui64, 536870912Ui64}}
    std::allocator<value_type> allocator{};

    std::cout<<std::endl<<std::endl<<"benchmark_memcpy_multithread"<<" threads_number "<<threads_number_type{}()<<std::endl;
    for (const auto& size : sizes){
        auto src = allocator.allocate(size);
        auto dst = allocator.allocate(size);
        std::iota(src, src+size, static_cast<value_type>(0));
        auto start = cpu_timer{};
        memcpy_multithread<threads_number_type{}()>(dst,src,size*sizeof(value_type));
        auto stop = cpu_timer{};

        auto dt_ms = stop - start;
        std::cout<<std::endl<<size_to_str<value_type>(size)<<" "<<bandwidth_to_str<value_type>(size, dt_ms)<<" copy timer "<<dt_ms;
        REQUIRE(std::equal(src, src+size, dst));
        allocator.deallocate(src,size);
        allocator.deallocate(dst,size);
    }
}