#ifndef CIRCULAR_VECTOR_HPP
#define CIRCULAR_VECTOR_HPP

#include <vector>
#include <cstdlib>
#include <cstring>

template<class T>
class CircularVector {
public:
	// TODO configurable default size?
	CircularVector() : is_empty(true), data(nullptr), i_start(0), i_end(0) {
		this->data = (T*)malloc(16 * sizeof(T));
		this->allocated = 16;
	};

	~CircularVector() {
		free(this->data);
	}

	void clear() noexcept {
		this->i_end = this->i_start;
		this->is_empty = true;
	}

	T & back() const {
		if (this->i_end > 0) {
			return this->data[this->i_end - 1];
		} else {
			return this->data[this->allocated - 1];
		}
	}

	T & front() const {
		return this->data[this->i_start];
	}

	template<class InnerT>
	void push_back(InnerT && value) {
		if (__builtin_expect((i_end == i_start) && (!this->is_empty), false)) {
			this->grow();
		}

		this->data[i_end] = std::forward<InnerT>(value);
		this->i_end++;
		if (this->i_end == this->allocated) {
			this->i_end = 0;
		}
		
		this->is_empty = false;
	}

	void pop_back() noexcept {
		if (__builtin_expect(this->i_end == 0, false)) {
			this->i_end = this->allocated - 1;
		} else {
			this->i_end--;
		}

		if (__builtin_expect(this->i_end == this->i_start, false)) {
			this->is_empty = true;
		}
	}

	void pop_front() noexcept {
		if (__builtin_expect(this->i_start == this->allocated - 1, false)) {
			this->i_start = 0;
		} else {
			this->i_start++;
		}
		
		if (__builtin_expect(this->i_end == this->i_start, false)) {
			this->is_empty = true;
		}
	}

	template<class InnerT>
	void push_front(InnerT && value) {
		if (__builtin_expect((i_end == i_start) && (!this->is_empty), false)) {
			this->grow();
		}

		// TODO after growing, the expectation is wrong.
		if (__builtin_expect((this->i_start == 0), false)) {
			this->i_start = this->allocated - 1;
		} else {
			this->i_start--;
		}

		this->data[this->i_start] = std::forward<InnerT>(value);

		this->is_empty = false;
	}

	T & operator[](size_t index) noexcept {
		return this->data[(this->i_start + index) % this->allocated];
	}

	size_t size() const noexcept {
		if (this->i_start < this->i_end) {
			return this->i_end - this->i_start;
		} else if (this->i_start > this->i_end) {
			return (this->allocated - this->i_start + this->i_end);
		} else if (!this->is_empty) {
			return this->allocated;
		} else {
			return 0;
		}
	}

	bool empty() const noexcept {
		return this->is_empty;
	}

private:
	void grow() {
		T * new_data = (T*) malloc(sizeof(T) * this->allocated * 2);

		// TODO copy into the middle?
		std::memcpy(new_data, this->data + this->i_start, this->allocated - this->i_start);
		std::memcpy(new_data + (this->allocated - this->i_start), this->data, this->i_end);

		free(this->data);
		this->data = new_data;
	}

	bool is_empty;
	T * data;
	size_t allocated;
	size_t i_start;
	size_t i_end;
};


#endif
