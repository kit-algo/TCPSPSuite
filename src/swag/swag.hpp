//
// Created by lukas on 28.06.18.
//

#ifndef TCPSPSUITE_SWAG_HPP
#define TCPSPSUITE_SWAG_HPP

#include "../datastructures/fast_reset_vector.hpp"
#include "../datastructures/skyline_interface.hpp" // for Sky...
#include "../instance/job.hpp"                     // for Job
#include "../instance/solution.hpp"                // for Sol...
#include "../instance/traits.hpp"
#include "../manager/solvers.hpp" // for get...
#include "../manager/timer.hpp"   // for Timer
#include "../util/log.hpp"        // for Log
#include "elitepoolscorer.hpp"
#include "matrixedgescorer.hpp"
#include <bitset>
#include <random>   // for mt1...
#include <stddef.h> // for size_t
#include <string>   // for string
#include <tuple>    // for tuple
#include <utility>  // for pair
#include <vector>   // for vector

class AdditionalResultStorage;
class Instance;
class SolverConfig;
namespace solvers {
template <unsigned int>
struct registry_hook;
}

namespace swag {
namespace detail {

class Edge {
public:
	Edge(Job::JobId target, size_t rev_index_in, bool permanent)
	    : t(target), rev_index(rev_index_in)
	{
		this->set_permanent(permanent);
		this->set_marked(false);
	}

	Edge() : t(0), rev_index(0) {}

	Job::JobId t;
	size_t rev_index;

	bool
	is_permanent() const noexcept
	{
		return this->flags[FLAG_INDEX_PERMANENT];
	}

	bool
	is_marked() const noexcept
	{
		return this->flags[FLAG_INDEX_MARKED];
	}
	void
	set_marked(bool value) noexcept
	{
		this->flags[FLAG_INDEX_MARKED] = value;
	}

	bool
	is_seen() const noexcept
	{
		return this->flags[FLAG_INDEX_SEEN];
	}
	void
	set_seen(bool value) noexcept
	{
		this->flags[FLAG_INDEX_SEEN] = value;
	}

	Edge &
	operator=(const Edge & other)
	{
		this->t = other.t;
		this->rev_index = other.rev_index;
		this->flags = other.flags;

		return *this;
	}

private:
	void
	set_permanent(bool value) noexcept
	{
		this->flags[FLAG_INDEX_PERMANENT] = value;
	}

	std::bitset<2> flags;
	static constexpr size_t FLAG_INDEX_PERMANENT = 0;
	static constexpr size_t FLAG_INDEX_MARKED = 1;
	static constexpr size_t FLAG_INDEX_SEEN = 2;
};

struct ReverseEdge
{
	Job::JobId s;
	size_t forward_index;

	ReverseEdge &
	operator=(const ReverseEdge & other)
	{
		this->s = other.s;
		this->forward_index = other.forward_index;

		return *this;
	}
};

template <bool use_mes, bool use_eps>
class SWAGSolver {
public:
	SWAGSolver(const Instance & instance, AdditionalResultStorage & additional,
	           const SolverConfig & sconf);
	void run();
	Solution get_solution();

	void dbg_verify();

private:
	void graph_insert_edge(Job::JobId s, Job::JobId t,
	                       bool permanent = false) noexcept;
	void graph_delete_edge(Job::JobId s, size_t s_adj_list_index) noexcept;
	void graph_delete_edge(Edge * e) noexcept;

	void initialize_graph() noexcept;
	void initialize_times() noexcept;
	void initialize_skyline() noexcept;

	/* Takes its input via push_es_forward_queue, outputs via
	 * changed_nodes_buf. */
	void push_es_forward(bool force_complete, bool range_changed) noexcept;
	void rebuild_es_forward() noexcept;

	/* Takes its input via push_lf_backwards_queue */
	void push_lf_backward(bool force_complete, bool range_changed) noexcept;
	void rebuild_lf_backward() noexcept;

	void insert_edge(Job::JobId s, Job::JobId t, bool force_complete) noexcept;

	void delete_edge(Edge * e) noexcept;
	void delete_edge(Job::JobId s, size_t s_adj_list_index) noexcept;

	bool iteration_insert_edge(bool force_complete) noexcept;
	void iteration_regenerate_candidates() noexcept;
	void iteration_unstick() noexcept;
	void iteration_propagate(bool complete, bool range_changed) noexcept;
	void iteration() noexcept;
	void reset() noexcept;

	/* Candidate building & selection */
	void build_candidate_jobs() noexcept;
	// Returns the score sum
	void build_candidate_edges() noexcept;
	void build_candidate_edges_batched() noexcept;
	size_t batch_offset;

	// Force-Creates new candidates by deleting edges
	void create_new_candidate_edges() noexcept;
	unsigned int find_edges_to_delete_backwards(Job::JobId t, unsigned int amount,
	                                            size_t max_depth);
	void edgedel_update_current_values_backwards(Job::JobId t);

	unsigned int find_edges_to_delete_forwards(Job::JobId s, unsigned int amount,
	                                           size_t max_depth);
	void edgedel_update_current_values_forwards(Job::JobId s);

	void bulk_delete();

	void dbg_verify_graph();
	void dbg_verify_cycle_free();
	void dbg_verify_solution();
	void dbg_verify_times();
	void dbg_generate_dot(std::ostringstream & buf);
	void dbg_print_graph();
	void dbg_write_graph(std::string filename);
	void dbg_print_adjacencies();
	void dbg_verify_active_range();

	void dbg_verify_values_during_forwards_bfs();
	void dbg_verify_values_during_backwards_bfs();
	void dbg_verify_limits_during_partial_propagation();
	void dbg_verify_correctly_partially_propagated();

	const Instance & instance;
	double timelimit;
	const SolverConfig & sconf;
	AdditionalResultStorage & additional;
	bool disaggregate_time;
	double intermediate_interval;
	double intermediate_score_interval;
	size_t deletion_trials;
	size_t deletion_max_depth;
	size_t deletions_before_reset;
	size_t force_complete_push_after;
	size_t force_range_check_after;
	bool randomize_edge_candidates;
	size_t edge_candidate_batchsize;
	double deletion_undermove_penalty;

	size_t deletions_remaining;
	size_t last_complete_push;
	size_t last_range_check;
	Timer run_timer;

	std::vector<std::vector<Edge>> adjacency_list;
	std::vector<std::vector<ReverseEdge>> rev_adjacency_list;

	ds::SkyLine rsl;

	std::vector<unsigned int> earliest_starts;
	std::vector<unsigned int> latest_finishs;

	std::vector<unsigned int> base_earliest_starts;
	std::vector<unsigned int> base_latest_finishs;
	std::vector<std::vector<Edge>> base_adjacency_list;
	std::vector<std::vector<ReverseEdge>> base_rev_adjacency_list;

	double best_score;
	std::vector<unsigned int> best_start_times;

	std::mt19937 rnd;

	/* Scoring */
	utilities::OptionalMember<MatrixEdgeScorer, use_mes> mes;
	utilities::OptionalMember<ElitePoolScorer, use_eps> eps;
	double score_sum;

	/* Statistics - Counts */
	size_t iteration_count;
	size_t insertion_count;
	size_t solution_count;
	size_t deletion_count;
	size_t reset_count;

	/* Statistics - Times */
	double skyline_update_time;
	double propagate_time;
	double reset_time;
	double job_selection_time;
	double edge_selection_time;
	double unstick_time;

	// Periodic logging
	Timer log_timer;
	double last_log_time;
	size_t last_log_iteration;

	/* Intermediate results */
	double intermediate_score_last_time;
	size_t intermediate_score_number;

	/* Caching this makes things a lot faster. */
	std::vector<decltype(Job().get_duration())> durations;
	std::vector<decltype(Job().get_deadline())> deadlines;
	std::vector<decltype(Job().get_release())> releases;
	const size_t job_count;

	/* Allocate-once buffers */
	std::vector<Job::JobId> candidates_buf;
	utilities::ConditionalMember<
	    std::vector<std::tuple<double, Job::JobId, Job::JobId>>,
	    std::vector<std::pair<Job::JobId, Job::JobId>>, use_mes || use_eps>
	    candidate_edge_buf;
	std::vector<std::pair<Job::JobId, unsigned int>> active_jobs_buf;

	std::pair<unsigned int, unsigned int> active_range;

	// TODO change queues to circular vector?
	std::vector<Job::JobId> push_lf_backward_queue;
	std::vector<Job::JobId> push_lf_backward_out_of_range;
	std::vector<Job::JobId> push_es_forward_queue;
	std::vector<Job::JobId> push_es_forward_out_of_range;

	// TODO change queues to circular vector?
	std::vector<Job::JobId> rebuild_es_forward_queue;
	std::vector<Job::JobId> rebuild_lf_backward_queue;

	std::vector<Job::JobId> changed_nodes_buf;
	FastResetVector<bool> node_moved_buf;

	/*
	 * Bulk deletion buffers
	 */
	// forward_deletion_buckets[s] stores the list of indices into
	// adjacency_list[s] that should be deleted
	std::vector<std::vector<size_t>> forward_deletion_buckets;
	// reverse_deletion_buckets[t] stores the list of indices into
	// rev_adjacency_list[t] that should be deleted
	std::vector<std::vector<size_t>> reverse_deletion_buckets;
	// forward_pointers_changed[s] stores (edge_index, new_rev_index), where
	// adjacency_list[s][edge_index].rev_index should be new_rev_index
	std::vector<std::vector<std::pair<size_t, size_t>>> forward_pointers_changed;
	// reverse_pointers_changed[t] stores (edge_index, new_forward_index), where
	// rev_adj_list[t][edge_index].forward_index should be new_forward_index
	std::vector<std::vector<std::pair<size_t, size_t>>> reverse_pointers_changed;

	/* Edge-deletion related buffers
	 *
	 * The set of edges that we currently consider for deletion consists of:
	 *
	 * * all marked edges in bfs_buf
	 * * all the edges in committed_deletion_buffer;
	 *
	 */
	struct EdgeBFSEntry
	{
		Edge * e;
		size_t depth;

		EdgeBFSEntry(Edge * e_in, size_t depth_in) : e(e_in), depth(depth_in) {}
		EdgeBFSEntry() : e(nullptr), depth(0) {}
	};

	/* For forward search:
	 * stores the length of the (taut) critical path from
	 * this->latest_finishs[wanted_t] to this->latest_finishs[i]
	 *
	 * For backward search:
	 * stores the length of the (taut) critical path from
	 * this->earliest_starts[i] to this->earliest_starts[wanted_s]
	 */

	/* Stores the length of the (taut) critical path of [i] to the
	 * wanted_t or wanted_s */
	// std::vector<unsigned int> edgedel_critical_paths_lengths;
	/* TODO */
	// std::vector<unsigned int> edgedel_worst_in_upstream;

	/* Stores whether the edgedel_critical_paths_lengths and
	 * edgedel_max_useful_movement
	 * for a vertex have already been set in this BFS */
	FastResetVector<bool> edgedel_vertex_seen;
	std::vector<Edge *> edgedel_edge_seen;

	// TODO
	std::vector<unsigned int> edgedel_current_value;
	std::vector<Job::JobId> edgedel_sorted_by_start_buf;
	std::vector<Job::JobId> edgedel_sorted_by_end_buf;

	/* TODO FIXME DEBUG */
	// CircularVector<EdgeBFSEntry> bfs_buf;
	std::deque<EdgeBFSEntry> bfs_buf;
	std::deque<Job::JobId> rebuild_queue;
	//	CircularVector<Job::JobId> rebuild_queue;
	// std::vector<EdgeBFSEntry> bfs_buf;
	// std::vector<Job::JobId> rebuild_queue;

	std::vector<Edge *> bfs_pruned_buffer;
	std::vector<Job::JobId> bfs_ran_out_of_buffer; // TODO do we still need this?

	std::vector<Edge *> delete_backwards_edges_buf;
	std::vector<Edge *> delete_forwards_edges_buf;

	Log l;
};

} // namespace detail

/*
 * This class acts only as a dispatcher to the correct specialization
 */
class SWAGSolver {
public:
	SWAGSolver(const Instance & instance, AdditionalResultStorage & additional,
	           const SolverConfig & sconf);
	void run();
	Solution get_solution();
	static std::string get_id();
	Maybe<double> get_lower_bound();
	static const Traits & get_requirements();

private:
	size_t impl_index;
	static const Traits required_traits;

	std::tuple<detail::SWAGSolver<false, false>, detail::SWAGSolver<false, true>,
	           detail::SWAGSolver<true, false>, detail::SWAGSolver<true, true>>
	    impl;
};
} // namespace swag

// Register the solver
namespace solvers {

template <>
struct registry_hook<solvers::get_free_N<swag::SWAGSolver>()>
{
	constexpr static unsigned int my_N = solvers::get_free_N<swag::SWAGSolver>();

	auto
	operator()()
	{
		return solvers::register_class<swag::SWAGSolver, my_N>{}();
	}
};

} // namespace solvers

#endif // TCPSPSUITE_SWAG_HPP
