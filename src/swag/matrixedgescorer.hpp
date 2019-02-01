#ifndef MATRIXEDGESCORER_HPP
#define MATRIXEDGESCORER_HPP

#include <cstddef>
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

class MatrixEdgeScorer {
public:
	MatrixEdgeScorer(const Instance & instance,
	                 const SolverConfig & sconf) noexcept;

	double get_score_for(size_t s, size_t t) const noexcept;
	void incorporate_result(
	    double quality, const std::vector<unsigned int> & start_times,
	    const std::vector<std::vector<detail::Edge>> & adjacency_list);
	void iteration(size_t iteration) noexcept;

private:
	const Instance & instance;

	size_t score_window_size;
	double score_window_fraction_threshold;
	double score_exponent;

	bool use_score_a;
	unsigned int aging_interval;
	unsigned int age_period;
	std::vector<std::pair<unsigned int, double>> score_matrix_a;
	std::vector<std::pair<unsigned int, double>> score_matrix_b;

	void adjust_score_for(unsigned int s, unsigned int t, double delta) noexcept;

	std::vector<double> last_scores;
	size_t last_scores_offset;
};

} // namespace swag

#endif
