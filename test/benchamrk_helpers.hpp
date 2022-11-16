#ifndef BENCHMARK_HELPERS_HPP_
#define BENCHMARK_HELPERS_HPP_

#include <chrono>

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

#endif