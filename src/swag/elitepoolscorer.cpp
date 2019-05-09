#include "elitepoolscorer.hpp"

#include "../instance/instance.hpp"
#include "../util/solverconfig.hpp"
#include "swag.hpp"

namespace swag {
using namespace detail;

ElitePoolScorer::ElitePoolScorer(const Instance & instance_in,
                                 const SolverConfig & sconf)
    : instance(instance_in), solutions_seen(0), sigmoid_base(M_E),
      sigmoid_coefficient(2), rng((unsigned long)sconf.get_seed()),
      best_score(std::numeric_limits<double>::max()), current_generation(1),
      num_replaced(0), l("EPS")
{
	if (sconf.has_config("pool_size")) {
		this->pool_size = (size_t)sconf["pool_size"].get<size_t>();
	} else {
		this->pool_size = 50;
	}

	if (sconf.has_config("start_factor")) {
		this->start_factor = (double)sconf["start_factor"].get<double>();
	} else {
		this->start_factor = 1.5;
	}

	if (sconf.has_config("sigmoid_base")) {
		this->sigmoid_base = sconf["sigmoid_base"].get<double>();
	}

	if (sconf.has_config("sigmoid_coefficient")) {
		this->sigmoid_coefficient = sconf["sigmoid_coefficient"].get<double>();
	}

	this->pool_start_times.resize(instance.job_count(),
	                              std::vector<unsigned int>(this->pool_size));
	this->scores.resize(this->pool_size);
	this->cache.resize(instance.job_count(),
	                   std::vector<CacheEntry>(instance.job_count(), {0, 0}));

	this->durations.resize(instance.job_count());
	for (unsigned int jid = 0; jid < instance.job_count(); ++jid) {
		this->durations[jid] = instance.get_job(jid).get_duration();
	}
}

double
ElitePoolScorer::get_score_for(size_t s, size_t t) const noexcept
{
	if ((double)this->solutions_seen <
	    this->start_factor * (double)this->pool_size) {
		return 1.0;
	}

	if (this->cache[s][t].generation != this->current_generation) {
		this->update_cache(s, t);
	}

	return (double)this->cache[s][t].i_before_j_count / (double)this->pool_size;
}

void
ElitePoolScorer::update_cache(size_t s, size_t t) const
{
	// TODO also update backward edge

	// TODO make sure that SIMD is used here
	size_t count = 0;
	for (size_t i = 0; i < this->pool_size; ++i) {
		if (this->pool_start_times[s][i] + this->durations[s] <=
		    this->pool_start_times[t][i]) {
			count++;
		}
	}

	this->cache[s][t].i_before_j_count = count;
	this->cache[s][t].generation = this->current_generation;
}

void
ElitePoolScorer::iteration(size_t iteration) noexcept
{
	(void)iteration;

	/* TODO remove this? */
	if (iteration % 5000 == 0) {
		BOOST_LOG(l.d(1)) << "Eviction percentage: "
		                  << (double)this->num_replaced / (double)this->solutions_seen;
	}
}

void
ElitePoolScorer::incorporate_result(
    double quality, const std::vector<unsigned int> & start_times,
    const std::vector<std::vector<Edge>> & adjacency_list)
{
	(void)adjacency_list;

	// While pool is not full, include everything!
	if (this->solutions_seen < this->pool_size) {
		this->replace_elite_pool(this->solutions_seen, quality, start_times);
		this->solutions_seen++;
		return;
	}

	this->solutions_seen++;

	/*
	 * Some basic rules:
	 *
	 * * The best solution is never evicted from the pool
	 * * A new best solution is always taken into the pool
	 * * Let S_p be the quality of a solution p in the pool, S_opt be the quality
	 * of the best solution, and S_i be the quality of a new solution. Then, p is
	 * evicted (by i) based on (S_p - S_opt) / (S_i - S_opt)
	 * * For S_i = S_p, the probability of being evicted should be 0.5
	 *
	 */

	std::uniform_real_distribution<double> distr(0.0, 1.0);

	for (size_t i = 0; i < this->pool_size; ++i) {
		size_t index = (i + this->solutions_seen) % this->pool_size;

		double SpSopt = this->scores[index] - best_score;
		if (SpSopt < EPS_DOUBLE_DELTA) {
			continue; // Best solution is protected
		}

		if (quality < this->best_score) {
			// Always take a new best solution
			this->replace_elite_pool(index, quality, start_times);
			std::cout << "Best Score: " << this->best_score << "\n";
			this->num_replaced++;
			break;
		}

		double SiSopt = quality - best_score;
		double t;
		if (SpSopt > SiSopt) {
			t = (SpSopt / SiSopt) - 1.0; // the sigmoid has p=0.5 at t=0
		} else {
			t = 1.0 - (SiSopt / SpSopt);
		}

		double prob = 1.0 / (1 + std::pow(this->sigmoid_base,
		                                  -1 * this->sigmoid_coefficient * t));
		double v = distr(this->rng);
		if (v < prob) {
			this->replace_elite_pool(index, quality, start_times);
			this->num_replaced++;
			break;
		}
	}
}

void
ElitePoolScorer::replace_elite_pool(
    size_t index, double quality, const std::vector<unsigned int> & start_times)
{
	for (unsigned int jid = 0; jid < this->instance.job_count(); ++jid) {
		this->pool_start_times[jid][index] = start_times[jid];
	}
	this->scores[index] = quality;
	if (quality < this->best_score) {
		this->best_score = quality;
	}

	this->current_generation++;
}

} // namespace swag
