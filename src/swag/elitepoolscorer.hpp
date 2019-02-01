#ifndef ELITEPOOLSCORER_HPP
#define ELITEPOOLSCORER_HPP

#include "../util/log.hpp"
#include <boost/container/flat_set.hpp>
#include <cstddef>
#include <random>
#include <vector>

// Forwards
class SolverConfig;
class Instance;
namespace swag {
namespace detail {
class Edge;
}
} // namespace swag

namespace swag {

class ElitePoolScorer {
public:
	ElitePoolScorer(const Instance & instance, const SolverConfig & sconf);

	double get_score_for(size_t s, size_t t) const noexcept;
	void incorporate_result(
	    double quality, const std::vector<unsigned int> & start_times,
	    const std::vector<std::vector<detail::Edge>> & adjacency_list);
	void iteration(size_t iteration) noexcept;

private:
	constexpr static double EPS_DOUBLE_DELTA = 0.0000001;

	const Instance & instance;

	void replace_elite_pool(size_t index, double quality,
	                        const std::vector<unsigned int> & start_times);

	size_t solutions_seen;

	double start_factor;
	size_t pool_size;
	double sigmoid_base;
	double sigmoid_coefficient;

	std::mt19937 rng; // TODO initialize!

	// start_times[jid][i] is the start time of job <jid> in the elite
	// solution nr. <i>
	// TODO make this one large vector, for better SIMDing
	std::vector<std::vector<unsigned int>> pool_start_times;

	// scores[i] is the quality of elite solution nr <i>
	std::vector<double> scores;
	double best_score;

	// TODO make this a half-matrix
	struct CacheEntry
	{
		size_t i_before_j_count; // TODO this should be the computed double
		size_t generation;
	};

	mutable std::vector<std::vector<CacheEntry>> cache;

	void update_cache(size_t s, size_t t) const;

	size_t current_generation;

	// This is just here for faster iteration
	std::vector<unsigned int> durations;

	/* Statistics */
	size_t num_replaced;

	Log l;
};

} // namespace swag

#endif
