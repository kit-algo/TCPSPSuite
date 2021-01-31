#include "randomizer.hpp"

#include <limits> // for numeric_limits

Randomizer::Randomizer(Maybe<int> global_seed_in)
{
	if (!global_seed_in.valid()) {
		std::random_device rd;
		// Overflow is okay here :)
		this->global_seed = (int)rd();
	} else {
		this->global_seed = global_seed_in;
	}

	this->rng = std::mt19937(static_cast<unsigned long>(this->global_seed));
	this->uni =
	    std::uniform_int_distribution<int>(0, std::numeric_limits<int>::max());
}

int
Randomizer::get_random()
{
	std::lock_guard<std::mutex> guard(this->lock);

	return this->uni(this->rng);
}

int
Randomizer::get_global_seed()
{
	return this->global_seed;
}
