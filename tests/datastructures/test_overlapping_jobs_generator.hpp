#ifndef TCPSPSUITE_TEST_OJG_HPP
#define TCPSPSUITE_TEST_OJG_HPP

#include <random>

using namespace testing;

#include "../src/algorithms/graphalgos.hpp"
#include "../src/datastructures/overlapping_jobs_generator.hpp"
#include "../src/instance/instance.hpp"
#include "../src/instance/job.hpp"
#include "../src/instance/laggraph.hpp"
#include "../src/instance/resource.hpp"

namespace test {
namespace overlapping_jobs_generator {

// chosen by fair xkcd
constexpr unsigned int TEST_SEED = 4;
constexpr unsigned int TEST_JOBCOUNT = 500;
constexpr double TEST_DEP_PROB = 0.01;

TEST(OverlappingJobsGeneratorTest, ComprehensiveTest)
{
	Instance ins;
	Resource res(0);
	res.set_investment_costs({{1.0, 1.0}});
	ins.add_resource(std::move(res));
	std::mt19937 rng(TEST_SEED);
	std::uniform_int_distribution<unsigned int> distr(0, 1000);

	std::vector<std::pair<unsigned int, unsigned int>> job_desc;
	for (size_t i = 0; i < TEST_JOBCOUNT; ++i) {
		unsigned int val_a = distr(rng);
		unsigned int val_b = distr(rng);
		while (val_b == val_a) {
			val_b = distr(rng);
		}

		if (val_a > val_b) {
			job_desc.emplace_back(val_b, val_a);
		} else {
			job_desc.emplace_back(val_a, val_b);
		}
	}

	unsigned int id = 0;
	for (const auto & [r, d] : job_desc) {
		// std::cout << "Job: " << id << ": " << r << " -> " << d << "\n";
		Job j(r, d, 1, {1.0}, id++);
		ins.add_job(std::move(j));
	}

	// Add some dependencies
	std::uniform_real_distribution<double> prob_distr(0, 1);
	for (unsigned int i = 0; i < job_desc.size(); ++i) {
		const Job & job_i = ins.get_job(i);
		for (unsigned int j = i + 1; i < job_desc.size(); ++i) {
			if (prob_distr(rng) < TEST_DEP_PROB) {
				const Job & job_j = ins.get_job(j);
				ins.get_laggraph().add_edge(
				    job_i, job_j, {static_cast<int>(job_i.get_duration()), 0, 0});
			}
		}
	}

	// Compute distinctness matrix
	std::vector<bool> distinct_matrix(TEST_JOBCOUNT * TEST_JOBCOUNT, false);
	for (unsigned int i = 0; i < job_desc.size(); ++i) {
		const auto & [r_i, d_i] = job_desc[i];
		for (unsigned int j = i + 1; j < job_desc.size(); ++j) {
			const auto & [r_j, d_j] = job_desc[j];

			//			// std::cout << "Comparing " << i << " vs " << j << ": " <<
			// job_desc[i] << " vs. " << job_desc[j] << "\n";
			if ((r_i >= d_j) || (r_j >= d_i)) {
				// std::cout << "Jobs do *NOT* overlap: " << i << " <=> " << j << "\n";
				distinct_matrix[i * TEST_JOBCOUNT + j] = true;
				distinct_matrix[j * TEST_JOBCOUNT + i] = true;
			}
		}
	}

	for (unsigned int i = 0; i < job_desc.size(); ++i) {
		DFS(
		    ins.get_laggraph(), i,
		    [&](auto v, auto from) {
			    (void)from;
			    if (v != i) {
				    // std::cout << " ## Path between " << v << " <=> " << i << "\n";
				    distinct_matrix[i * TEST_JOBCOUNT + v] = true;
				    distinct_matrix[v * TEST_JOBCOUNT + i] = true;
			    }
			    return true;
		    },
		    [](auto) {}, [](auto, auto, auto) {});
	}

	std::vector<bool> non_overlapping_matrix(TEST_JOBCOUNT * TEST_JOBCOUNT, true);
	OverlappingJobsGenerator ojg(ins);

	for (const auto & [s, t] : ojg) {
		// std::cout << "Pair: " << s << " <-> " << t << "\n";
		non_overlapping_matrix[s * TEST_JOBCOUNT + t] = false;
		non_overlapping_matrix[t * TEST_JOBCOUNT + s] = false;
	}
	for (size_t i = 0; i < TEST_JOBCOUNT; ++i) {
		non_overlapping_matrix[i * TEST_JOBCOUNT + i] = false;
	}

	for (size_t i = 0; i < non_overlapping_matrix.size(); ++i) {
		// std::cout << i << "\n" << std::flush;
		ASSERT_EQ(distinct_matrix[i], non_overlapping_matrix[i]);
	}
}

} // namespace overlapping_jobs_generator
} // namespace test

#endif
