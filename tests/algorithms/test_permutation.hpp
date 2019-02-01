//
// Created by lukas on 12.02.18.
//

#ifndef TCPSPSUITE_TEST_PERMUTATION_HPP
#define TCPSPSUITE_TEST_PERMUTATION_HPP

#include <random>
#include <algorithm>

#include "../../src/algorithms/sorting.hpp"

namespace test {
namespace algo {

constexpr unsigned int TEST_SEED = 4;
constexpr unsigned int SIZE = 1000;

class PermutationTest : public Test
{
public:
	virtual void SetUp() {
		this->rng = std::mt19937(TEST_SEED);

		this->data.clear();
		this->indices.clear();

		for (unsigned int i = 0 ; i < SIZE ; ++i) {
			this->data.push_back(i);
			this->indices.push_back(i);
		}
	}

	void shuffle() {
		std::shuffle(this->indices.begin(), this->indices.end(), this->rng);
	}

	std::mt19937 rng;
	std::vector<unsigned int> data;
	std::vector<unsigned int> indices;
};

TEST_F(PermutationTest, TestApplication) {
	shuffle();

	::algo::apply_permutation(data, indices);

	for (unsigned int i = 0 ; i < SIZE ; ++i) {
		ASSERT_EQ(data[i], indices[i]);
  }
}

TEST(SpecialPermutationTest, TestSingleElement) {
	std::vector<unsigned int> data;
	std::vector<unsigned int> indices;

	indices.push_back(0);
	data.push_back(0);

	::algo::apply_permutation(data, indices);
}

TEST(SpecialPermutationTest, TestTwoCycles) {
	/* Cycle 1: 0 -> 1 -> 4
	 * Cycle 2: 2 -> 3 -> 5
	 */
	std::vector<unsigned int> indices {4, 0, 5, 2, 1, 3};
	std::vector<unsigned int> data {0, 1, 2, 3, 4, 5};

	::algo::apply_permutation(data, indices);

	for (unsigned int i = 0 ; i < 6 ; ++i) {
		ASSERT_EQ(data[i], indices[i]);
	}
}

TEST(SpecialPermutationTest, TestUnchanged) {
	std::vector<unsigned int> indices {0, 1, 2, 3, 4, 5};
	std::vector<unsigned int> data {0, 1, 2, 3, 4, 5};

	::algo::apply_permutation(data, indices);

	for (unsigned int i = 0 ; i < 6 ; ++i) {
		ASSERT_EQ(data[i], i);
	}
}

} // namespace test::algo
} // namespace test

#endif //TCPSPSUITE_TEST_PERMUTATION_HPP

