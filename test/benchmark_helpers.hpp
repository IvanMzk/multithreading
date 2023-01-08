#ifndef BENCHMARK_HELPERS_HPP_
#define BENCHMARK_HELPERS_HPP_

#include <chrono>
#include <sstream>

namespace benchmark_helpers{

class cpu_timer
{
    using clock_type = std::chrono::steady_clock;
    using time_point = typename clock_type::time_point;
    time_point point_;
public:
    cpu_timer():
        point_{clock_type::now()}
    {}
    friend auto operator-(const cpu_timer& end, const cpu_timer& start){
        return std::chrono::duration<float,std::milli>(end.point_-start.point_).count();
    }
};

template<std::size_t N, std::size_t N_groups>
struct make_ranges_helper{
    template<std::size_t...I>
    constexpr static auto make_ranges(std::index_sequence<I...>){return std::array<std::size_t,sizeof...(I)>{make_range<I>()...};}
    template<std::size_t I>
    constexpr static auto make_range(){return I*(N/N_groups);}
    template<>
    constexpr static auto make_range<N_groups>(){return N_groups*(N/N_groups)+(N%N_groups);}
};

template<std::size_t N, std::size_t N_groups>
constexpr auto make_ranges(){
    return make_ranges_helper<N,N_groups>::make_ranges(std::make_index_sequence<N_groups+1>{});
}

template<std::size_t Init, std::size_t Fact>
constexpr auto make_size_helper(std::size_t i){
    if (i==0){
        return Init;
    }else{
        return Fact*make_size_helper<Init,Fact>(i-1);
    }
}
template<std::size_t Init, std::size_t Fact, std::size_t...I>
auto constexpr make_sizes_helper(std::index_sequence<I...>){
    return std::array<std::size_t, sizeof...(I)>{make_size_helper<Init,Fact>(I)...};
}
template<std::size_t Init = (1<<20), std::size_t Fact = 2, std::size_t N>
auto constexpr make_sizes(){
    return make_sizes_helper<Init,Fact>(std::make_index_sequence<N>{});
}

template<typename T>
auto size_in_bytes(std::size_t n){return n*sizeof(T);}
template<typename T>
auto size_in_mbytes(std::size_t n){return size_in_bytes<T>(n)/std::size_t{1000000};}
template<typename T>
auto size_in_gbytes(std::size_t n){return size_in_bytes<T>(n)/std::size_t{1000000000};}
template<typename T>
auto size_to_str(std::size_t n){
    std::stringstream ss{};
    ss<<size_in_mbytes<T>(n)<<"MByte";
    return ss.str();
}

template<typename T>
auto bandwidth_in_gbytes(std::size_t n, float dt_ms){
    return size_in_mbytes<T>(n)/dt_ms;
}
template<typename T>
auto bandwidth_to_str(std::size_t n, float dt_ms){
    std::stringstream ss{};
    ss<<bandwidth_in_gbytes<T>(n,dt_ms)<<"GBytes/s";
    return ss.str();
}

}   //end of namespace benchmark_helpers


#endif