#ifndef CIRCULAR_BUFFER_HPP_
#define CIRCULAR_BUFFER_HPP_

#include <array>

namespace experimental_multithreading{

template<typename T, std::size_t N>
class circular_buffer
{
public:
    using value_type = T;
    circular_buffer():
        elements_{},
        push_cursor{0},
        pop_cursor{0}
    {}

    void push(const value_type& v){

    }

    auto size()const{
        
    }

private:
    std::array<value_type,N> elements_;
    std::atomic<std::size_t> push_cursor;
    std::atomic<std::size_t> pop_cursor;
};

}   //end of namespace experimental_multithreading


#endif