//
// Created by lukas on 19.03.18.
//

#ifndef TCPSPSUITE_TEST_SP_HPP
#define TCPSPSUITE_TEST_SP_HPP

#include <random>

#include "../../src/state_propagation/statepropagation.hpp"
#include "../src/util/solverconfig.hpp"
#include "../src/db/storage.hpp"
#include "../src/instance/laggraph.hpp"
#include "../src/manager/errors.hpp"

using namespace testing;

namespace test {
namespace state_propagation {

constexpr unsigned int TEST_SEED = 4;

class SPTest : public Test
{
public:
	SPTest()
					: sconf("SPTest", "ID", {}, { 20u }, { 1 })
	{}

	virtual void
	SetUp()
	{
		instance = Instance();
		rng = std::mt19937(TEST_SEED);
	}

	void run_sp() {
		AdditionalResultStorage aresult;
		statepropagation::StatePropagation<statepropagation::SPOptions_None> sp(instance, aresult, sconf);
		sp.run();
		this->sol = sp.get_solution();
	}

	SolverConfig sconf;
	Solution sol;
	Instance instance;
	std::mt19937 rng;
};

TEST_F(SPTest, TrivialTest) {
	Resource res(0);
	res.set_investment_costs({{1.0, 1.0}});

	this->instance.add_resource(std::move(res));

	Job j0(0, 10, 10, { 1.0 }, 0);
	Job j1(20, 30, 10, { 1.0 }, 1);

	instance.add_job(std::move(j0));
	instance.add_job(std::move(j1));

	this->run_sp();

	this->sol.verify(sconf.get_seed());

	ASSERT_EQ(sol.get_max_usage(0), 1.0);
	ASSERT_EQ(sol.get_start_time(0), 0);
	ASSERT_EQ(sol.get_start_time(1), 20);
}

TEST_F(SPTest, BasicTest) {
Resource res(0);
res.set_investment_costs({{1.0, 1.0}});

this->instance.add_resource(std::move(res));

Job j0(0, 10, 5, { 1.0 }, 0);
Job j1(0, 10, 5, { 1.0 }, 1);
Job j2(9, 10, 1, { 2.0 }, 2);

Job j3(100, 200, 10, { 1.0 }, 3);
Job j4(100, 200, 10, { 1.0 }, 4);
Job j5(100, 101, 1, { 1.0 }, 5);
Job j6(199, 200, 1, { 3.0 }, 6);


instance.add_job(std::move(j0));
instance.add_job(std::move(j1));
instance.add_job(std::move(j2));
instance.add_job(std::move(j3));
instance.add_job(std::move(j4));
instance.add_job(std::move(j5));
instance.add_job(std::move(j6));

this->run_sp();

this->sol.verify(sconf.get_seed());

ASSERT_EQ(sol.get_max_usage(0), 3.0);
}

} // namespace state_propagation
} // namespace test

#endif //TCPSPSUITE_TEST_SP_HPP
