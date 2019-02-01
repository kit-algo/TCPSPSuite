//
// Created by lukas on 22.03.18.
//

#ifndef TCPSPSUITE_TEST_PROPAGATOR_HPP
#define TCPSPSUITE_TEST_PROPAGATOR_HPP


#include <random>

#include "../../src/state_propagation/ds.hpp"
#include "../../src/state_propagation/statepropagation.hpp"
#include "../src/util/solverconfig.hpp"
#include "../src/db/storage.hpp"
#include "../src/instance/laggraph.hpp"
#include "../src/manager/errors.hpp"

using namespace testing;
using namespace statepropagation;

namespace test {
namespace state_propagation {

TEST(PropagationStatus, FullGenerationTest) {
				std::vector<StaticData::JobStartData> data = {{0, 20}, {1, 10}, {2, 9}, {3, 8}, {4, 7},
				                                              {5,6},{6,5},{7,4},{8,3},{9,2}};
				std::vector<bool> ignore(data.size(), false);
				PropagationStatus ps;
				ps.reset(&data, std::move(ignore), {}, std::numeric_limits<double>::max());
				unsigned int count = 1;

				std::set<std::vector<Job::JobId>> seen;
				seen.insert(ps.get_active_jobs());

				while (ps.advance(std::numeric_limits<double>::max())) {
					ASSERT_EQ(seen.find(ps.get_active_jobs()), seen.end());
					seen.insert(ps.get_active_jobs());
					/*
					std::cout << "--- Possibility: ";
					for (auto jid : ps.get_active_jobs()) {
						std::cout << jid << ", ";
					}
					std::cout << "\n";
					 */
					count++;
				}
				ASSERT_EQ(count, (int)std::pow(2,data.size()));
}

TEST(PropagationStatus, IgnoreTest) {
	std::vector<StaticData::JobStartData> data = {{0, 20}, {1, 10}, {2, 9}, {3, 8}, {4, 7},
	                                              {5,6},{6,5},{7,4},{8,3},{9,2}};
	std::vector<bool> ignore(data.size(), false);
	ignore[9] = true;
	ignore[0] = true;
	ignore[4] = true;

	PropagationStatus ps;
	ps.reset(&data, std::move(ignore), {}, std::numeric_limits<double>::max());
	unsigned int count = 1;

	std::set<std::vector<Job::JobId>> seen;
	seen.insert(ps.get_active_jobs());

	while (ps.advance(std::numeric_limits<double>::max())) {
		ASSERT_EQ(seen.find(ps.get_active_jobs()), seen.end());

		auto it = std::find(ps.get_active_jobs().begin(), ps.get_active_jobs().end(), 4);
		ASSERT_EQ(it, ps.get_active_jobs().end());

		it = std::find(ps.get_active_jobs().begin(), ps.get_active_jobs().end(), 0);
		ASSERT_EQ(it, ps.get_active_jobs().end());

		it = std::find(ps.get_active_jobs().begin(), ps.get_active_jobs().end(), 9);
		ASSERT_EQ(it, ps.get_active_jobs().end());
		/*
		std::cout << "--- Possibility: ";
		for (auto jid : ps.get_active_jobs()) {
			std::cout << jid << ", ";
		}
		std::cout << "\n";
		 */
		count++;
	}
	ASSERT_EQ(count, (int)std::pow(2,data.size() - 3));
}

// TODO FIXME add more tests!


} // namespace state_propagation
} // namespace test


#endif //TCPSPSUITE_TEST_PROPAGATOR_HPP
