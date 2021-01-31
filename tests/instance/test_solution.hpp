//
// Created by lukas on 07.12.17.
//

#ifndef TCPSPSUITE_TEST_SOLUTION_HPP
#define TCPSPSUITE_TEST_SOLUTION_HPP

#include <random>

using namespace testing;

#include "../src/instance/instance.hpp"
#include "../src/instance/resource.hpp"
#include "../src/instance/solution.hpp"

namespace test {
namespace instance {

TEST(SolutionTest, TestOvershootWithFlatAvailability)
{
	Instance instance;
	Availability av(10);
	Resource res(0);
	res.set_availability(std::move(av));

	res.set_overshoot_costs(FlexCost({
	    {1.0, 1.0},
	}));

	Resource res1(1);
	Availability av1(5);
	res1.set_availability(std::move(av1));
	res1.set_overshoot_costs(FlexCost({
	    {2.0, 2.0},
	}));

	instance.add_resource(std::move(res));
	instance.add_resource(std::move(res1));

	Job j0(0, 10, 10, {12.0, 6.0}, 0);
	instance.add_job(std::move(j0));
	Job j1(20, 30, 1, {27.0, 7.0}, 1);
	instance.add_job(std::move(j1));

	Solution sol(instance, true, {{0}, {20}}, Maybe<double>());

	ASSERT_EQ(sol.get_max_usage(0), 17);
	ASSERT_EQ(sol.get_max_usage(1), 2);
	ASSERT_EQ(sol.get_costs(), 65); // (10 * 2 + 1 * 17) + (10 * 2 + 1 * 8)
}

TEST(SolutionTest, TestFlexibleOvershoot)
{
	Instance instance;
	Availability av(10);
	Resource res(0);
	res.set_availability(std::move(av));

	FlexCost overshoot_cost({});
	overshoot_cost.set_flexible({{0,
	                              {
	                                  {1.0, 1.0},
	                              }},
	                             {1,
	                              {
	                                  {10.0, 1.0},
	                              }},
	                             {2, {{5, 1.0}, {1.0, 3}}}});

	res.set_overshoot_costs(std::move(overshoot_cost));

	instance.add_resource(std::move(res));

	Job j0(0, 1, 1, {11.0}, 0);
	instance.add_job(std::move(j0));
 	Job j1(1, 2, 1, {12.0}, 1);
	instance.add_job(std::move(j1));
 	Job j2(2, 4, 2, {13.0}, 1);
	instance.add_job(std::move(j2));

	Solution sol(instance, true, {{0}, {1}, {2}}, Maybe<double>());

	ASSERT_EQ(sol.get_costs(), 105);
	// 1.0 * 1.0^1.0 +
	// 10.0 * 2.0^1.0 +
	// 2 * (5 * 3.0^1.0 + 1.0 * 3.0^3)
}

TEST(SolutionTest, TestInvestmentWithFlatAvailability)
{
	Instance instance;
	Availability av(10);
	Resource res(0);
	res.set_availability(std::move(av));
	res.set_investment_costs({
	    {1.0, 1.0},
	});

	Resource res1(1);
	Availability av1(5);
	res1.set_availability(std::move(av1));
	res1.set_investment_costs({
	    {2.0, 2.0},
	});

	instance.add_resource(std::move(res));
	instance.add_resource(std::move(res1));

	Job j0(0, 10, 10, {12.0, 6.0}, 0);
	instance.add_job(std::move(j0));
	Job j1(20, 30, 1, {27.0, 7.0}, 1);
	instance.add_job(std::move(j1));

	Solution sol(instance, true, {{0}, {20}}, Maybe<double>());

	ASSERT_EQ(sol.get_max_usage(0), 17);
	ASSERT_EQ(sol.get_max_usage(1), 2);
	ASSERT_EQ(sol.get_costs(), 25); // (1 * 17) + (2 * 2^2)
}

TEST(SolutionTest, TestOvershootWithChangingAvailability)
{
	Instance instance;
	Availability av(0);
	av.set({{0, 5.0}, {10, 10.0}, {20, 15.0}, {30, 0.0}});
	Resource res(0);
	res.set_availability(std::move(av));
	res.set_overshoot_costs(FlexCost({
	    {1.0, 1.0},
	}));

	instance.add_resource(std::move(res));

	// Job aligned with an availability segment. Runs from 0 to 10, overshoots
	// by 1
	Job j0(0, 10, 10, {6.0}, 0);
	instance.add_job(std::move(j0));

	// Job starting and ending between av. segments. Runs from 15 to 25,
	// overshoots 7 in the first segment, 2 in the second
	Job j1(15, 25, 10, {17}, 1);
	instance.add_job(std::move(j1));

	Solution sol(instance, true, {{0}, {15}}, Maybe<double>());

	ASSERT_EQ(sol.get_max_usage(0), 7);
	ASSERT_EQ(sol.get_costs(), 55); // (10 * 1 + 5 * 7 + 5 * 2)
}

} // namespace instance
} // namespace test

#endif // TCPSPSUITE_TEST_SOLUTION_HPP
