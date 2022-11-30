#ifndef FOUR_POINTERS_CIRCULAR_BUFFER_HPP_
#define FOUR_POINTERS_CIRCULAR_BUFFER_HPP_

#include <atomic>
#include <array>

namespace experimental_multithreading{

//push to the tail, pop from head

template<typename T, std::size_t N>
class four_pointers_circular_buffer
{
public :
    using value_type = T;
    using size_type = std::size_t;
    static constexpr std::size_t buffer_size = N;

	four_pointers_circular_buffer() = default;

	auto try_push(const value_type& v) {
		/* Find a place to write */
		//can't try push, must push at pos, pos is reserved by this call, wait(spin loop) until push at pos is possible
		//if try push and cant push in this call, then pos is missed and value not stored, next succesful push make this missed slot valid to pop...
		size_type pos = r_tail.fetch_add(1);
		if (head_.load() + buffer_size <=  pos){//prevent cyclic overwriting
			return false;
		}
		if (head_.load() + buffer_size > pos){

		}

        /* do the write */
		elements_[pos%buffer_size] = v;

        /* stay you did the write */
		tail_.fetch_add(1);

		return true;
	}

	auto try_pop(value_type& v) {
		// choose a position to read from
		size_type  pos = r_head.fetch_add(1);

        // don't read from the tail...
		if (pos >= tail_.load()){
			std::cout<<std::endl<<pos<<" "<<tail_.load();
			return false;
		}

		v = elements_[pos%buffer_size];
		head_.fetch_add(1);
		return true;
	}

	// void push(const value_type& v) {
	// 	/* Find a place to write */
	// 	size_type pos = r_tail.fetch_add(1);
	// 	while (head() + buffer_size <=  pos);

    //     /* do the write */
	// 	elements_[pos%buffer_size] = v;

    //     /* stay you did the write */
	// 	tail_.fetch_add(1);
	// }

	// void pop(value_type& v) {
	// 	// choose a position to read from
	// 	size_type  pos = r_head.fetch_add(1);

    //     // don't read from the tail...
	// 	while (pos >= tail());

	// 	v = elements_[pos%buffer_size];
	// 	head_.fetch_add(1);
	// }
private:

    std::atomic<size_type> head_;
	std::atomic<size_type> tail_;
	std::atomic<size_type> r_head;
	std::atomic<size_type> r_tail;
    std::array<value_type, buffer_size> elements_;
};

}   //end of namespace experimental_multithreading

#endif