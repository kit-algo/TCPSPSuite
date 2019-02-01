#include "matrixedgescorer.hpp"

#include "../instance/instance.hpp"
#include "../util/solverconfig.hpp"
#include "swag.hpp"

namespace swag {
using namespace detail;

MatrixEdgeScorer::MatrixEdgeScorer(const Instance & instance_in,
                                   const SolverConfig & sconf) noexcept
    : instance(instance_in), score_window_size(10),
      score_window_fraction_threshold(0.5), score_exponent(2),
      use_score_a(true), aging_interval(0), age_period(1),
      score_matrix_a(instance_in.job_count() * instance_in.job_count(), {0, 0}),
      score_matrix_b(instance_in.job_count() * instance_in.job_count(), {0, 0}),
      last_scores(score_window_size, 0), last_scores_offset(0)
{
	if (sconf.has_config("score_window_size")) {
		this->score_window_size = (size_t)sconf["score_window_size"];
		last_scores.resize(score_window_size, 0);
	}

	if (sconf.has_config("score_window_fraction_threshold")) {
		this->score_window_fraction_threshold =
		    (double)sconf["score_window_fraction_threshold"];
	}

	if (sconf.has_config("score_exponent")) {
		this->score_exponent = (double)sconf["score_exponent"];
	}

	if (sconf.has_config("aging_interval")) {
		this->aging_interval = (unsigned int)sconf["aging_interval"];
	}
}

double
MatrixEdgeScorer::get_score_for(size_t s, size_t t) const noexcept
{
	if (this->aging_interval == 0) {
		return this->score_matrix_a[s * this->instance.job_count() + t].second;
	}

	double score = 1.0;

	const auto & entry_a =
	    this->score_matrix_a[s * this->instance.job_count() + t];
	if (entry_a.first >= this->age_period - 1) {
		score += entry_a.second;
	}
	const auto & entry_b =
	    this->score_matrix_b[s * this->instance.job_count() + t];
	if (entry_b.first >= this->age_period - 1) {
		score += entry_b.second;
	}

	return score;
}

void
MatrixEdgeScorer::iteration(size_t iteration_count) noexcept
{
	if ((this->aging_interval > 0) &&
	    (iteration_count % this->aging_interval == 0)) {
		this->use_score_a = !this->use_score_a;
		this->age_period++;
	}
}

void
MatrixEdgeScorer::adjust_score_for(unsigned int s, unsigned int t,
                                   double delta) noexcept
{
	if (this->use_score_a) {
		auto & entry = this->score_matrix_a[s * this->instance.job_count() + t];
		if (entry.first != this->age_period) {
			entry.first = this->age_period;
			entry.second = delta;
		} else {
			entry.second += delta;
		}
	} else {
		auto & entry = this->score_matrix_b[s * this->instance.job_count() + t];
		if (entry.first != this->age_period) {
			entry.first = this->age_period;
			entry.second = delta;
		} else {
			entry.second += delta;
		}
	}
}

void
MatrixEdgeScorer::incorporate_result(
    double score, const std::vector<unsigned int> & start_times,
    const std::vector<std::vector<Edge>> & adjacency_list)
{
	(void)start_times;

	size_t better_scores = 0;
	for (unsigned int i = 0; i < this->score_window_size; i++) {
		if (score >= this->last_scores[i]) {
			better_scores++;
		}
	}

	double score_adjustment = 0;
	if ((double)better_scores <
	    (double)this->score_window_size * this->score_window_fraction_threshold) {
		score_adjustment = 1 - ((double)better_scores /
		                        ((double)((double)this->score_window_size *
		                                  this->score_window_fraction_threshold)));
	}

	this->last_scores[this->last_scores_offset] = score;
	this->last_scores_offset++;
	this->last_scores_offset %= this->score_window_size;

	for (size_t s = 0; s < this->instance.job_count(); ++s) {
		for (auto & edge : adjacency_list[s]) {
			if (!edge.is_permanent()) {
				this->adjust_score_for((unsigned int)s, edge.t, score_adjustment);
			}
		}
	}
}

} // namespace swag
