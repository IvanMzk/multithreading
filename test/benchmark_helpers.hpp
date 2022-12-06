#ifndef BENCHMARK_HELPERS_HPP_
#define BENCHMARK_HELPERS_HPP_

#include <chrono>

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

}   //end of namespace benchmark_helpers


#endif