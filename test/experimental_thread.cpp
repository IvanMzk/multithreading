#include <thread>
#include <iostream>
#include "catch.hpp"

namespace exprerimental_thread{

auto thread_id(){
    return std::this_thread::get_id();
}

class A{
    int a;
public:
    explicit A(int a_ = 0):
        a{a_}
    {
        std::cout<<std::endl<<"explicit A(int a_ = 0):";
        std::cout<<std::endl<<"thread id "<<thread_id();
    }
    A(const A& other):
        a{other.a}
    {
        std::cout<<std::endl<<"A(const A& other):";
        std::cout<<std::endl<<"thread id "<<thread_id();
    }
    A(A&& other):
        a{other.a}
    {
        std::cout<<std::endl<<"A(A&& other):";
        std::cout<<std::endl<<"thread id "<<thread_id();
    }
};

auto f(A a){
    std::cout<<std::endl<<"thread id "<<thread_id();
}
auto g(const A& a){
    std::cout<<std::endl<<"thread id "<<thread_id();
}
auto h(A& a){
    std::cout<<std::endl<<"thread id "<<thread_id();
}


}   //end of namespace exprerimental_thread

TEST_CASE("thread_function_parameters","[exprerimental_thread]"){
    using exprerimental_thread::f;
    using exprerimental_thread::g;
    using exprerimental_thread::h;
    using exprerimental_thread::A;
    A a{};
    //std::thread t(f, a);
    std::thread t(g, A{});
    t.join();
}