#ifndef MAYBE_H
#define MAYBE_H

// TODO c++17 replaces this with std::optional
template <class T>
class Maybe {
public:
	Maybe(T val_in) : val(val_in), is_valid(true) {}

	Maybe() : is_valid(false) {}

	bool
	valid() const
	{
		return this->is_valid;
	}

	T &
	value()
	{
		// TODO assertion?
		return this->val;
	}

	const T &
	value() const
	{
		// TODO assertion?
		return this->val;
	}

	operator const T &() const
	{
		// TODO assertion?
		return this->val;
	}

	T &
	value_or_default(T & def)
	{
		if (this->is_valid) {
			return this->val;
		} else {
			return def;
		}
	}

	const T &
	value_or_default(const T & def)
	{
		if (this->is_valid) {
			return this->val;
		} else {
			return def;
		}
	}

	const T &
	value_or_default(const T & def) const
	{
		if (this->is_valid) {
			return this->val;
		} else {
			return def;
		}
	}

private:
	T val;
	bool is_valid;
};

#endif
