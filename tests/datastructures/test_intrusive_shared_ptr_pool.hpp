//
// Created by lukas on 23.05.18.
//

#ifndef TCPSPSUITE_TEST_INTRUSIVE_SHARED_PTR_POOL_HPP
#define TCPSPSUITE_TEST_INTRUSIVE_SHARED_PTR_POOL_HPP

#include <random>

using namespace testing;

#include "../src/datastructures/intrusive_shared_ptr.hpp"

namespace test {
namespace intrusive_shared_ptr {

class Dummy {
public:
	int a;
};

using Pool = SharedPtrPool<Dummy>;

class SharedPtrPoolTest : public Test
{
public:
	virtual void SetUp() {

	}

	Pool p;
};

TEST_F(SharedPtrPoolTest, TestConstruction) {
}

} // namespace test::skyline
} // namespace test

#endif //TCPSPSUITE_TEST_INTRUSIVE_SHARED_PTR_POOL_HPP
