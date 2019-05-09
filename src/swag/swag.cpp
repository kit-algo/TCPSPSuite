#include "swag.hpp"

#include "../datastructures/maybe.hpp"
#include "../datastructures/skyline.hpp"
#include "../datastructures/skyline_interface.hpp" // for SkyLine
#include "../db/storage.hpp"
#include "../instance/instance.hpp"
#include "../instance/job.hpp" // for Job::JobId, Job
#include "../instance/laggraph.hpp"
#include "../instance/resource.hpp"
#include "../instance/solution.hpp"
#include "../instance/traits.hpp"
#include "../manager/errors.hpp"
#include "../manager/timer.hpp"
#include "../util/fault_codes.hpp"
#include "../util/log.hpp" // for Log
#include "../util/solverconfig.hpp"
#include <algorithm>
#include <boost/container/vector.hpp>
#include <cassert>
#include <cmath>
#include <ext/alloc_traits.h>
#include <limits>
#include <memory>

namespace swag {
namespace detail {

template <bool use_mes, bool use_eps>
SWAGSolver<use_mes, use_eps>::SWAGSolver(
    const Instance & instance_in, AdditionalResultStorage & additional_in,
    const SolverConfig & sconf_in)
    : instance(instance_in), sconf(sconf_in), additional(additional_in),
      disaggregate_time(false), intermediate_interval(0),
      intermediate_score_interval(0), deletion_trials(30),
      deletion_max_depth(6), deletions_before_reset(30),
      force_complete_push_after(50), force_range_check_after(0),
      randomize_edge_candidates(false), edge_candidate_batchsize(0),
      deletion_undermove_penalty(3), last_complete_push(0), last_range_check(0),
      adjacency_list(instance_in.job_count()),
      rev_adjacency_list(instance_in.job_count()),
      rsl((instance.resource_count() > 1)
              ? ds::SkyLine{ds::RangedTreeSkyLine{&instance}}
              : ds::SkyLine{ds::SingleRangedTreeSkyLine{&instance}}),
      earliest_starts(instance_in.job_count()),
      latest_finishs(instance_in.job_count()),
      best_score(std::numeric_limits<double>::max()),
      best_start_times(instance_in.job_count(), 0),
      rnd((unsigned long)sconf.get_seed()), mes(instance_in, sconf_in),
      eps(instance_in, sconf_in), insertion_count(0), solution_count(0),
      deletion_count(0), reset_count(0), skyline_update_time(0),
      propagate_time(0), reset_time(0), job_selection_time(0),
      edge_selection_time(0), unstick_time(0), last_log_time(0),
      last_log_iteration(0), intermediate_score_last_time(0),
      intermediate_score_number(0), job_count(instance.job_count()),
      node_moved_buf(job_count, false),
      forward_deletion_buckets(instance.job_count()),
      reverse_deletion_buckets(instance.job_count()),
      forward_pointers_changed(instance.job_count()),
      reverse_pointers_changed(instance.job_count()),
      edgedel_vertex_seen(instance.job_count(), false),
      edgedel_current_value(instance.job_count()), l("E-INS")
{
	if (!this->sconf.get_time_limit().valid()) {
		BOOST_LOG(l.e()) << "SWAGSolver needs a time limit!";
		throw ConfigurationError(instance_in, sconf.get_seed(),
		                         FAULT_TIME_LIMIT_NEEDED,
		                         "SWAGSolver needs a time limit!");
	}
	this->timelimit = this->sconf.get_time_limit();

	if (this->sconf.has_config("disaggregate_time")) {
		this->disaggregate_time = (bool)this->sconf["disaggregate_time"];
	}

	if (this->sconf.has_config("deletion_trials")) {
		this->deletion_trials = (size_t)this->sconf["deletion_trials"];
	}

	if (this->sconf.has_config("deletions_before_reset")) {
		this->deletions_before_reset =
		    (size_t)this->sconf["deletions_before_reset"];
	}

	if (this->sconf.has_config("complete_propagation_after")) {
		this->force_complete_push_after =
		    (size_t)this->sconf["complete_propagation_after"];
	}

	if (this->sconf.has_config("force_range_check_after")) {
		this->force_range_check_after =
		    (size_t)this->sconf["force_range_check_after"];
	}

	if (this->sconf.has_config("randomize_edge_candidates")) {
		this->randomize_edge_candidates =
		    (bool)this->sconf["randomize_edge_candidates"];
	}

	if (this->sconf.has_config("edge_candidate_batchsize")) {
		this->edge_candidate_batchsize =
		    (size_t)this->sconf["edge_candidate_batchsize"];
	}

	if (this->sconf.has_config("deletion_max_depth")) {
		this->deletion_max_depth = (size_t)this->sconf["deletion_max_depth"];
	}

	if (this->sconf.has_config("intermediate_interval")) {
		this->intermediate_interval =
		    (unsigned int)this->sconf["intermediate_interval"];
	}

	if (this->sconf.has_config("intermediate_score_interval")) {
		this->intermediate_score_interval =
		    (double)this->sconf["intermediate_score_interval"];
	}

	if (this->sconf.has_config("deletion_undermove_penalty")) {
		this->deletion_undermove_penalty =
		    (double)this->sconf["deletion_undermove_penalty"];
	}

	this->durations.resize(this->job_count);
	this->deadlines.resize(this->job_count);
	this->releases.resize(this->job_count);
	for (Job::JobId jid = 0; jid < this->job_count; ++jid) {
		const Job & job = this->instance.get_job(jid);
		this->durations[jid] = job.get_duration();
		this->deadlines[jid] = job.get_deadline();
		this->releases[jid] = job.get_release();
	}

	this->active_range = {0, 0};
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::bulk_delete()
{
	for (Job::JobId jid = 0; jid < this->job_count; ++jid) {
		this->forward_pointers_changed[jid].clear();
		this->reverse_pointers_changed[jid].clear();
	}

	/* Deletion happens in three passes:
	 * 1. delete edges from forward adjacency lists, storing the changes to the
	 * reverse adjacency list
	 * 2. apply changes to reverse adjacency lists and delete from reverse
	 * adjacency lists, storing changes to the forward adjacency list
	 * 3. apply changes to forward adjacency lists
	 */

	// Pass 1
	for (Job::JobId s = 0; s < this->job_count; ++s) {
		if (this->forward_deletion_buckets[s].empty()) {
			continue;
		}

		/* The same index might appear multiple times in the list of
		 * indices to be deleted. Obviously, we should delete only
		 * once, so we deduplicate. */
		size_t last_index = std::numeric_limits<size_t>::max();

		/* Inside the adjacency list, we must delete from the right to the left
		 * to ensure that an element is never moved twice (that would break
		 * the values in reverse_pointers_changed). */
		std::sort(this->forward_deletion_buckets[s].begin(),
		          this->forward_deletion_buckets[s].end(),
		          [](size_t lhs, size_t rhs) { return rhs < lhs; });

		size_t end_index = this->adjacency_list[s].size();

		for (size_t delete_index : this->forward_deletion_buckets[s]) {
			if (delete_index == last_index) {
				continue;
			}
			last_index = delete_index;

			if (delete_index != end_index - 1) {
				this->adjacency_list[s][delete_index] =
				    this->adjacency_list[s][end_index - 1];
				this->reverse_pointers_changed[this->adjacency_list[s][delete_index].t]
				    .push_back({this->adjacency_list[s][delete_index].rev_index,
				                delete_index});
			}
			end_index--;
		}

		this->adjacency_list[s].resize(end_index);
	}

	// Pass 2
	for (Job::JobId t = 0; t < this->job_count; ++t) {
		if (!this->reverse_pointers_changed[t].empty()) {
			for (auto [index, new_forward_index] :
			     this->reverse_pointers_changed[t]) {
				this->rev_adjacency_list[t][index].forward_index = new_forward_index;
			}
		}

		if (this->reverse_deletion_buckets[t].empty()) {
			continue;
		}

		/* Inside the adjacency list, we must delete from the right to the left
		 * to ensure that an element is never moved twice (that would break
		 * the values in reverse_pointers_changed). */
		std::sort(this->reverse_deletion_buckets[t].begin(),
		          this->reverse_deletion_buckets[t].end(),
		          [](size_t lhs, size_t rhs) { return rhs < lhs; });

		size_t last_index = std::numeric_limits<size_t>::max();
		size_t end_index = this->rev_adjacency_list[t].size();

		for (size_t delete_index : this->reverse_deletion_buckets[t]) {
			if (delete_index == last_index) {
				continue;
			}

			last_index = delete_index;

			if (delete_index != end_index - 1) {
				this->rev_adjacency_list[t][delete_index] =
				    this->rev_adjacency_list[t][end_index - 1];
				this->forward_pointers_changed[this->rev_adjacency_list[t][delete_index]
				                                   .s]
				    .push_back({this->rev_adjacency_list[t][delete_index].forward_index,
				                delete_index});
			}
			end_index--;
		}

		this->rev_adjacency_list[t].resize(end_index);
	}

	for (Job::JobId s = 0; s < this->job_count; ++s) {
		if (!this->forward_pointers_changed[s].empty()) {
			for (auto [index, new_rev_index] : this->forward_pointers_changed[s]) {
				this->adjacency_list[s][index].rev_index = new_rev_index;
			}
		}
	}
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::create_new_candidate_edges() noexcept
{
	/*
	std::cout << "=======================================================\n";
	std::cout << "=======================================================\n";
	std::cout << "====        Creating new candidate edges          =====\n";
	std::cout << "=======================================================\n";
	std::cout << "=======================================================\n";

	for (unsigned int jid = 0; jid < this->instance.job_count(); ++jid) {
	  std::cout << "ES[" << std::to_string(jid) << ": "
	            << std::to_string(this->earliest_starts[jid]);
	  std::cout << "  LF: " << std::to_string(this->latest_finishs[jid]) << "\n";
	}
	*/

	this->edgedel_sorted_by_start_buf = this->candidates_buf;
	this->edgedel_sorted_by_end_buf = this->candidates_buf;

	// std::cout << "# candidate jobs: " << this->candidates_buf.size() << "\n";

	std::sort(this->edgedel_sorted_by_end_buf.begin(),
	          this->edgedel_sorted_by_end_buf.end(),
	          [&](const Job::JobId lhs, const Job::JobId rhs) {
		          return this->earliest_starts[lhs] + this->durations[lhs] <
		                 this->earliest_starts[rhs] + this->durations[rhs];
	          });
	std::sort(this->edgedel_sorted_by_start_buf.begin(),
	          this->edgedel_sorted_by_start_buf.end(),
	          [&](const Job::JobId lhs, const Job::JobId rhs) {
		          return this->earliest_starts[lhs] < this->earliest_starts[rhs];
	          });

	for (size_t trial = 0;
	     (trial < this->deletion_trials) &&
	     ((trial / 2) + (trial % 2) < this->candidates_buf.size());
	     ++trial) {

		// std::cout << "!!!! Trial " << trial << "\n";
		/*
		 * Step 1 : Select jobs between which to look for a new candidate
		 * edge
		 */
		Job::JobId wanted_s = this->edgedel_sorted_by_end_buf[trial / 2];
		Job::JobId wanted_t =
		    this->edgedel_sorted_by_start_buf[(trial / 2) + (trial % 2)];

		// std::cout << ">>> Wanting: " << wanted_s << " -> " << wanted_t << "\n";
		if (wanted_s == wanted_t) {
			continue;
		}

		// TODO compensate for skipped?
		if ((this->earliest_starts[wanted_s] + this->durations[wanted_s] <
		     this->earliest_starts[wanted_t]) ||
		    (this->earliest_starts[wanted_t] + this->durations[wanted_t] <
		     this->earliest_starts[wanted_s])) {
			// Dont't overlap
			// std::cout << ">>>> Nope b/c no overlap.\n";
			continue;
		}

		if (this->base_earliest_starts[wanted_s] + this->durations[wanted_s] >
		    this->base_latest_finishs[wanted_t]) {
			// Not a chance.
			// std::cout << ">>>> Nope b/c too tight in base.\n";
			continue;
		}

		/*
		 * Step 2: Try to delete edges s.t. this pair of jobs can be
		 * moved enough.
		 */
		unsigned int overlap = this->earliest_starts[wanted_s] +
		                       this->durations[wanted_s] -
		                       this->earliest_starts[wanted_t];
		// std::cout << ">>>> Overlap: " << overlap << "\n";
		unsigned int wanted_backwards_amount =
		    std::min(overlap / 2, this->earliest_starts[wanted_s] -
		                              this->base_earliest_starts[wanted_s]);

		unsigned int backwards_amount =
		    (unsigned int)this->find_edges_to_delete_backwards(
		        wanted_s, wanted_backwards_amount, this->deletion_max_depth);

		// std::cout << ">>>> Backwards amount: " << backwards_amount << "\n";
		unsigned int wanted_forwards_amount = overlap - backwards_amount;
		unsigned int forwards_amount =
		    (unsigned int)this->find_edges_to_delete_forwards(
		        wanted_t, wanted_forwards_amount, this->deletion_max_depth);
		// std::cout << ">>>> Forwards amount: " << forwards_amount << "\n";

		if (forwards_amount < wanted_forwards_amount) {
			// Can't remove edges s.t. we can shift the jobs enough
			continue;
		}

		// From this point on, we are committed to deleting an edge
		this->deletion_count++;

		/* Step 3:
		 * We found a feasible deletion set. Delete all the edges,
		 * then propagate ES / LF values.
		 */
		//		std::cout << "-- Starting deletion.\n";
		/* Note that we can't just delete the edges iteratively, as that
		 * would wreak havoc on the pointers we're holding. Thus, we first
		 * store all (s, adjacency_list_index) to be deleted, and then delete in
		 * bulk.
		 */
		// TODO can't we directly store in this format?
		// TODO fast reset?
		for (Job::JobId jid = 0; jid < this->job_count; ++jid) {
			this->forward_deletion_buckets[jid].clear();
			this->reverse_deletion_buckets[jid].clear();
		}

		for (Edge * e : this->delete_forwards_edges_buf) {
			//			std::cout << "<< Edge marked for deletion: " << (size_t)e << "\n";
			Job::JobId s = this->rev_adjacency_list[e->t][e->rev_index].s;
			size_t forward_index =
			    this->rev_adjacency_list[e->t][e->rev_index].forward_index;

			this->rebuild_lf_backward_queue.push_back(s);
			this->rebuild_es_forward_queue.push_back(e->t);

			assert(this->adjacency_list[s].size() > forward_index);
			assert(this->rev_adjacency_list[e->t].size() > e->rev_index);

			this->forward_deletion_buckets[s].push_back(forward_index);
			this->reverse_deletion_buckets[e->t].push_back(e->rev_index);
		}

		for (Edge * e : this->delete_backwards_edges_buf) {
			Job::JobId s = this->rev_adjacency_list[e->t][e->rev_index].s;
			size_t forward_index =
			    this->rev_adjacency_list[e->t][e->rev_index].forward_index;

			this->rebuild_lf_backward_queue.push_back(s);
			this->rebuild_es_forward_queue.push_back(e->t);

			assert(this->adjacency_list[s].size() > forward_index);
			assert(this->rev_adjacency_list[e->t].size() > e->rev_index);

			this->forward_deletion_buckets[s].push_back(forward_index);
			this->reverse_deletion_buckets[e->t].push_back(e->rev_index);
		}

		this->bulk_delete();

		this->changed_nodes_buf.clear();
		this->rebuild_es_forward();
		this->rebuild_lf_backward();

		this->node_moved_buf.reset();
		Timer skyline_timer;
		if (this->disaggregate_time) {
			skyline_timer.start();
		}
		for (auto jid : this->changed_nodes_buf) {
			if (!this->node_moved_buf[jid]) {
				this->rsl.set_pos(jid, (int)this->earliest_starts[jid]);
				this->node_moved_buf[jid] = true;
			}
		}
		if (this->disaggregate_time) {
			this->skyline_update_time += skyline_timer.get();
		}

		/*
		 *Step 4: Insert the new candidate into the candidate edge set.
		 */
		if constexpr (use_mes || use_eps) {
			this->candidate_edge_buf.emplace_back(1.0, wanted_s, wanted_t);
			break; // We succeeded
		} else {
			this->candidate_edge_buf.emplace_back(wanted_s, wanted_t);
			break; // We succeeded
		}
	}
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::edgedel_update_current_values_backwards(
    Job::JobId initial_t)
{
	this->rebuild_queue.clear();
	this->rebuild_queue.push_back(initial_t);

	while (!this->rebuild_queue.empty()) {
		Job::JobId t = this->rebuild_queue[0];
		this->rebuild_queue.pop_front();

		unsigned max_es = this->base_earliest_starts[t];

		for (const auto & rev_edge : this->rev_adjacency_list[t]) {
			const auto & edge =
			    this->adjacency_list[rev_edge.s][rev_edge.forward_index];
			if (!edge.is_marked()) {
				if (this->edgedel_vertex_seen[rev_edge.s]) {
					max_es = std::max(max_es, this->edgedel_current_value[rev_edge.s] +
					                              this->durations[rev_edge.s]);
				} else {
					max_es = std::max(max_es, this->earliest_starts[rev_edge.s] +
					                              this->durations[rev_edge.s]);
				}
			}
		}
		if (max_es != this->edgedel_current_value[t]) {
			this->edgedel_current_value[t] = max_es;
			for (const auto & next_edge : this->adjacency_list[t]) {
				Job::JobId next_t = next_edge.t;
				if (this->edgedel_vertex_seen[next_t]) {
					// this makes this a heuristic. We might not see some paths!
					this->rebuild_queue.push_back(next_t);
				}
			}
		}
	}
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::edgedel_update_current_values_forwards(
    Job::JobId initial_s)
{
	// TODO evaluate whether a queue might actually help here!
	this->rebuild_queue.clear();
	this->rebuild_queue.push_back(initial_s);

	while (!this->rebuild_queue.empty()) {
		Job::JobId s = this->rebuild_queue[0];
		this->rebuild_queue.pop_front();

		unsigned min_lf = this->base_latest_finishs[s];

		for (const auto & edge : this->adjacency_list[s]) {
			if (!edge.is_marked()) {
				if (this->edgedel_vertex_seen[edge.t]) {
					assert(this->edgedel_current_value[edge.t] >=
					       this->durations[edge.t]);
					min_lf = std::min(min_lf, this->edgedel_current_value[edge.t] -
					                              this->durations[edge.t]);
				} else {
					min_lf = std::min(min_lf, this->latest_finishs[edge.t] -
					                              this->durations[edge.t]);
				}
			}
		}
		if (min_lf != this->edgedel_current_value[s]) {
			this->edgedel_current_value[s] = min_lf;
			for (const auto & next_edge : this->rev_adjacency_list[s]) {
				Job::JobId next_s = next_edge.s;
				if (this->edgedel_vertex_seen[next_s]) {
					// TODO this makes this a heuristic. We might not see some paths!
					this->rebuild_queue.push_back(next_s);
				}
			}
		}
	}
	/*
	if (cycle_detected) {
	  std::cout
	      << "--------------- Got out of the cycle! -----------------------\n";
	}
	*/
}

template <bool use_mes, bool use_eps>
unsigned int
SWAGSolver<use_mes, use_eps>::find_edges_to_delete_backwards(
    Job::JobId t, unsigned int amount, size_t depth)
{
#ifdef OMG_VERIFY
	// Make sure that no edges are marked
	for (Job::JobId jid = 0; jid < this->job_count; ++jid) {
		for (auto & edge : this->adjacency_list[jid]) {
			assert(!edge.is_marked());
			assert(!edge.is_seen());
		}
	}
#endif

	this->delete_backwards_edges_buf.clear();

	if (this->earliest_starts[t] - this->base_earliest_starts[t] == 0) {
		// We will never move anything
		return 0;
	}

	// Initialize buffers and values
	this->bfs_buf.clear();
	this->bfs_pruned_buffer.clear();
	this->bfs_ran_out_of_buffer.clear();
	this->edgedel_vertex_seen.reset();
	this->edgedel_edge_seen.clear();
	this->edgedel_vertex_seen[t] = true;
	// We start by deleting all incoming edges, so that really is the
	// current value for t
	this->edgedel_current_value[t] = this->base_earliest_starts[t];

	size_t moved_time_steps =
	    this->earliest_starts[t] - this->base_earliest_starts[t];
	size_t edges_removed = 0;
	// First step in the BFS
	for (const auto & rev_e : this->rev_adjacency_list[t]) {
		Edge * e = &(this->adjacency_list[rev_e.s][rev_e.forward_index]);

		this->edgedel_current_value[rev_e.s] = this->earliest_starts[rev_e.s];
		this->edgedel_vertex_seen[rev_e.s] = true;

		e->set_seen(true);
		this->edgedel_edge_seen.push_back(e);

		if (!e->is_permanent()) {
			// Check if we should remove this edge
			e->set_marked(true);
			edges_removed++;
			this->delete_backwards_edges_buf.push_back(e);
			this->bfs_buf.push_back(EdgeBFSEntry(e, 0));
		} else {
			moved_time_steps = (unsigned int)std::min(
			    (int)moved_time_steps,
			    ((int)this->earliest_starts[t] -
			     (int)(this->earliest_starts[rev_e.s] + this->durations[rev_e.s])));
			// We want to get rid of permanent edges as soon as possible
			this->bfs_buf.push_front(EdgeBFSEntry(e, 0));
		}
	}

	// Now, t has moved.
	this->edgedel_update_current_values_backwards(t);

	/* Scoring a deletion set is done as follows:
	 * - every edge in the set counts 1
	 * - every time step *below* "amount" counts
	 * (this->deletion_undermove_penalty)
	 */
	double best_deletion_score =
	    (double)edges_removed +
	    (double)std::max(((int)moved_time_steps - (int)amount), 0) *
	        (this->deletion_undermove_penalty);
	size_t best_moved_time_steps = moved_time_steps;

	while (!this->bfs_buf.empty()) {
		const auto candidate = this->bfs_buf[0];
		this->bfs_buf.pop_front();
		if ((candidate.depth >= depth) || (candidate.e->is_seen())) {
			// Prune the BFS here, which means that we permanently decide to
			// delete this edge, if it is not permanent
			if (!candidate.e->is_permanent()) {
				this->bfs_pruned_buffer.push_back(candidate.e);
			}
			continue;
		}

		candidate.e->set_seen(true);
		this->edgedel_edge_seen.push_back(candidate.e);

		if (candidate.e->is_marked()) {
			// This edge is no longer to be deleted.
			edges_removed--;
			candidate.e->set_marked(false);
			this->edgedel_update_current_values_backwards(candidate.e->t);
		}

		/* Perform one step in the Edge BFS:
		 *
		 * - Remove the (a,b)-edge from the BFS buffer (we did that already above)
		 * - Insert all incoming edges of a into the BFS buffer
		 *
		 * a will be called "sub_t" in the following.
		 */
		// The new target is the source of the edge we are replacing
		Job::JobId sub_t =
		    this->rev_adjacency_list[candidate.e->t][candidate.e->rev_index].s;
		assert(this->edgedel_vertex_seen[sub_t]);

		/* If the vertex has no incoming edges, the BFS naturally ends at this
		 * vertex. However, for computing the score later, we still need to compute
		 * critical paths starting in sub_t here. Thus, we remember sub_t.
		 */
		if (this->rev_adjacency_list[sub_t].empty()) {
			this->bfs_ran_out_of_buffer.push_back(sub_t);
		}

		for (const auto & rev_e : this->rev_adjacency_list[sub_t]) {
			Edge * e = &(this->adjacency_list[rev_e.s][rev_e.forward_index]);

			if (!e->is_permanent()) {
				// To be deleted!
				e->set_marked(true);
				edges_removed++;
				this->bfs_buf.push_back(EdgeBFSEntry(e, candidate.depth + 1));
			} else {
				// We want to get rid of permament edges ASAP
				this->bfs_buf.push_front(EdgeBFSEntry(e, candidate.depth + 1));
			}

			if (!this->edgedel_vertex_seen[rev_e.s]) {
				// *Set* the current value, as the vertex was never seen
				this->edgedel_current_value[rev_e.s] = this->earliest_starts[rev_e.s];
			}

			this->edgedel_vertex_seen[rev_e.s] = true;
		}

		// Something might have changed!
		this->edgedel_update_current_values_backwards(sub_t);

		/* After every BFS step: evaluate the quality of what we currently have.
		 * Quality is measured in two criteria:
		 *
		 * - Most important is the number of time steps we can move, up to the
		 * requested amount
		 * - After that, the less edges we delete, the better.
		 */
		assert(this->earliest_starts[t] >= this->edgedel_current_value[t]);
		moved_time_steps =
		    this->earliest_starts[t] - this->edgedel_current_value[t];

		double score = (double)edges_removed +
		               (double)std::max(((int)moved_time_steps - (int)amount), 0) *
		                   (this->deletion_undermove_penalty);

		if (score < best_deletion_score) {
			// New best solution
			best_deletion_score = score;
			best_moved_time_steps = moved_time_steps;

			this->delete_backwards_edges_buf.clear();
			for (Edge * e : this->bfs_pruned_buffer) {
				if (e->is_marked()) {
					this->delete_backwards_edges_buf.push_back(e);
				}
			}
			for (size_t cand_index = 0; cand_index < this->bfs_buf.size();
			     ++cand_index) {
				const auto & cand = this->bfs_buf[cand_index];

				if (cand.e->is_marked()) {
					this->delete_backwards_edges_buf.push_back(cand.e);
				}
			}
		}
	}

	/* Unmark everything that is still in the buffers */
	for (Edge * e : this->bfs_pruned_buffer) {
		e->set_marked(false);
	}
	while (!this->bfs_buf.empty()) {
		this->bfs_buf.back().e->set_marked(false);
		this->bfs_buf.pop_back();
	}
	for (Edge * e : this->edgedel_edge_seen) {
		e->set_seen(false);
	}

#ifdef OMG_VERIFY
	/* Mark everything that we suggest to delete, verify one final time that
	 * we can achieve what we suggest. */
	for (Edge * e : this->delete_backwards_edges_buf) {
		e->set_marked(true);
	}

	this->edgedel_vertex_seen.reset();
	this->edgedel_vertex_seen[t] = true;
	this->edgedel_current_value[t] =
	    this->earliest_starts[t] - best_moved_time_steps;

	for (Edge * e : this->delete_backwards_edges_buf) {
		e->set_marked(false);
	}
#endif

	return (unsigned int)best_moved_time_steps;
}

template <bool use_mes, bool use_eps>
unsigned int
SWAGSolver<use_mes, use_eps>::find_edges_to_delete_forwards(Job::JobId s,
                                                            unsigned int amount,
                                                            size_t depth)
{
#ifdef OMG_VERIFY
	// Make sure that no edges are marked
	for (Job::JobId jid = 0; jid < this->job_count; ++jid) {
		for (auto & edge : this->adjacency_list[jid]) {
			assert(!edge.is_marked());
			assert(!edge.is_seen());
		}
	}
#endif

	this->delete_forwards_edges_buf.clear();

	if (this->base_latest_finishs[s] - this->latest_finishs[s] == 0) {
		// We will never move anything
		return 0;
	}

	// Initialize buffers and values
	this->bfs_buf.clear();
	this->edgedel_edge_seen.clear();
	this->bfs_pruned_buffer.clear();
	this->bfs_ran_out_of_buffer.clear();
	this->edgedel_vertex_seen.reset();
	this->edgedel_vertex_seen[s] = true;
	// We start by deleting all outgoing edges, so that really is the
	// current value for s
	this->edgedel_current_value[s] = this->base_latest_finishs[s];

	size_t edges_removed = 0;
	size_t moved_time_steps =
	    this->base_latest_finishs[s] - this->latest_finishs[s];
	// First step in the BFS
	for (auto & e : this->adjacency_list[s]) {
		this->edgedel_current_value[e.t] = this->latest_finishs[e.t];
		this->edgedel_vertex_seen[e.t] = true;
		e.set_seen(true);
		this->edgedel_edge_seen.push_back(&e);

		if (!e.is_permanent()) {
			// Check if we should remove this edge
			e.set_marked(true);
			edges_removed++;
			this->delete_forwards_edges_buf.push_back(&e);
			this->bfs_buf.push_back(EdgeBFSEntry(&e, 0));
		} else {
			assert(this->latest_finishs[s] <=
			       (this->latest_finishs[e.t] - this->durations[e.t]));

			moved_time_steps = (unsigned int)std::min(
			    (int)moved_time_steps,
			    ((int)(this->latest_finishs[e.t] - this->durations[e.t]) -
			     (int)this->latest_finishs[s]));
			this->bfs_buf.push_front(EdgeBFSEntry(&e, 0));
		}
	}

	// Now, s has moved
	this->edgedel_update_current_values_forwards(s);

	/* Here, a scoring is not necessary - anything that moves less
	 * than <amount> time steps is unacceptable!
	 */
	size_t best_edges_removed = edges_removed;
	size_t best_moved_time_steps = moved_time_steps;

	while (!this->bfs_buf.empty()) {
		const auto candidate = this->bfs_buf[0];
		this->bfs_buf.pop_front();

		if ((candidate.depth >= depth) || (candidate.e->is_seen())) {
			// Prune the BFS here, which means that we permanently decide to
			// delete this edge, if it is not permanent
			if (!candidate.e->is_permanent()) {
				this->bfs_pruned_buffer.push_back(candidate.e);
			}
			continue;
		}

		if (candidate.e->is_marked()) {
			// This edge is no longer to be deleted.
			edges_removed--;
			candidate.e->set_marked(false);
			auto & rev_edge =
			    this->rev_adjacency_list[candidate.e->t][candidate.e->rev_index];
			this->edgedel_update_current_values_forwards(rev_edge.s);
		}

		// Mark the edge as seen s.t. we only run into it once during the BFS
		candidate.e->set_seen(true);
		this->edgedel_edge_seen.push_back(candidate.e);

		/* Perform one step in the Edge BFS:
		 *
		 * - Remove the (a,b)-edge from the BFS buffer (we did that already above)
		 * - Insert all outgoing edges of b into the BFS buffer
		 *
		 * b will be called "sub_s" in the following.
		 */
		// The new target is the source of the edge we are replacing
		Job::JobId sub_s = candidate.e->t;
		assert(this->edgedel_vertex_seen[sub_s]);

		/* If the vertex has no incoming edges, the BFS naturally ends at this
		 * vertex. However, for computing the score later, we still need to compute
		 * critical paths starting in sub_t here. Thus, we remember sub_t.
		 */
		if (this->adjacency_list[sub_s].empty()) {
			this->bfs_ran_out_of_buffer.push_back(sub_s);
		}

		for (auto & e : this->adjacency_list[sub_s]) {

			if (!e.is_permanent()) {
				// To be deleted!
				e.set_marked(true);
				edges_removed++;
				this->bfs_buf.push_back(EdgeBFSEntry(&e, candidate.depth + 1));
			} else {
				this->bfs_buf.push_front(EdgeBFSEntry(&e, candidate.depth + 1));
			}

			if (!this->edgedel_vertex_seen[e.t]) {
				// *Set* the current value, as the vertex was never seen
				this->edgedel_current_value[e.t] = this->latest_finishs[e.t];
			}

			this->edgedel_vertex_seen[e.t] = true;
		}

		// Something might have changed!
		this->edgedel_update_current_values_forwards(sub_s);

		/* After every BFS step: evaluate the quality of what we currently have.
		 * Quality is measured in two criteria:
		 *
		 * - Most important is the number of time steps we can move, up to the
		 * requested amount
		 * - After that, the less edges we delete, the better.
		 */
		assert(this->latest_finishs[s] <= this->edgedel_current_value[s]);
		moved_time_steps = this->edgedel_current_value[s] - this->latest_finishs[s];

		if ((moved_time_steps >= amount) && (edges_removed < best_edges_removed)) {
			// New best solution
			best_moved_time_steps = moved_time_steps;
			best_edges_removed = edges_removed;

			this->delete_forwards_edges_buf.clear();
			for (Edge * e : this->bfs_pruned_buffer) {
				if (e->is_marked()) {
					this->delete_forwards_edges_buf.push_back(e);
				}
			}
			for (size_t cand_index = 0; cand_index < this->bfs_buf.size();
			     ++cand_index) {
				const auto & cand = this->bfs_buf[cand_index];

				if (cand.e->is_marked()) {
					this->delete_forwards_edges_buf.push_back(cand.e);
				}
			}
		}
	}

	/* Unmark everything that is still in the buffers */
	for (Edge * e : this->bfs_pruned_buffer) {
		e->set_marked(false);
	}
	while (!this->bfs_buf.empty()) {
		this->bfs_buf.back().e->set_marked(false);
		this->bfs_buf.pop_back();
	}
	for (Edge * e : this->edgedel_edge_seen) {
		e->set_seen(false);
	}

#ifdef OMG_VERIFY
	/* Mark everything that we suggest to delete, verify one final time that
	 * we can achieve what we suggest. */
	for (Edge * e : this->delete_forwards_edges_buf) {
		e->set_marked(true);
	}

	this->edgedel_vertex_seen.reset();
	this->edgedel_vertex_seen[s] = true;
	this->edgedel_current_value[s] =
	    this->latest_finishs[s] + best_moved_time_steps;

	for (Edge * e : this->delete_forwards_edges_buf) {
		e->set_marked(false);
	}
#endif

	return (unsigned int)best_moved_time_steps;
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::graph_insert_edge(Job::JobId s, Job::JobId t,
                                                bool permanent) noexcept
{
	this->rev_adjacency_list[t].push_back({s, this->adjacency_list[s].size()});
	this->adjacency_list[s].emplace_back(
	    t, this->rev_adjacency_list[t].size() - 1, permanent);
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::graph_delete_edge(
    Job::JobId s, size_t s_adj_list_index) noexcept
{
	auto & edge = this->adjacency_list[s][s_adj_list_index];
	Job::JobId t = edge.t;
	auto rev_edge_index = edge.rev_index;

	this->adjacency_list[s][s_adj_list_index] = this->adjacency_list[s].back();
	this->rev_adjacency_list[t][rev_edge_index] =
	    this->rev_adjacency_list[t].back();
	this->adjacency_list[s][s_adj_list_index].rev_index = rev_edge_index;
	this->rev_adjacency_list[t][rev_edge_index].forward_index = s_adj_list_index;

	this->adjacency_list[s].pop_back();
	this->rev_adjacency_list[t].pop_back();
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::graph_delete_edge(Edge * e) noexcept
{
	Job::JobId t = e->t;
	auto rev_edge_index = e->rev_index;
	Job::JobId s = this->rev_adjacency_list[t][rev_edge_index].s;

	auto adj_edge_index =
	    this->rev_adjacency_list[t][rev_edge_index].forward_index;

	this->adjacency_list[s][adj_edge_index] = this->adjacency_list[s].back();
	this->rev_adjacency_list[t][rev_edge_index] =
	    this->rev_adjacency_list[t].back();
	this->adjacency_list[s][adj_edge_index].rev_index = rev_edge_index;
	this->rev_adjacency_list[t][rev_edge_index].forward_index = adj_edge_index;

	this->adjacency_list[s].pop_back();
	this->rev_adjacency_list[t].pop_back();
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::initialize_graph() noexcept
{
	for (auto & edge : this->instance.get_laggraph().edges()) {
		auto s = edge.s;
		auto t = edge.t;

		this->graph_insert_edge(s, t, true);
	}

	this->base_adjacency_list.resize(this->job_count);
	this->base_rev_adjacency_list.resize(this->job_count);
	for (size_t s = 0; s < this->job_count; ++s) {
		this->base_adjacency_list[s] = this->adjacency_list[s];
		this->base_rev_adjacency_list[s] = this->rev_adjacency_list[s];
	}
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::initialize_times() noexcept
{
	this->latest_finishs = this->deadlines;
	this->earliest_starts = this->releases;

	for (Job::JobId jid = 0; jid < this->job_count; ++jid) {
		this->push_es_forward_queue.push_back(jid);
		this->push_lf_backward_queue.push_back(jid);
	}
	this->push_es_forward(true, false);
	this->push_lf_backward(true, false);

	this->base_earliest_starts = this->earliest_starts;
	this->best_start_times = this->earliest_starts;
	this->base_latest_finishs = this->latest_finishs;
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::initialize_skyline() noexcept
{
	for (Job::JobId jid = 0; jid < this->job_count; ++jid) {
		this->rsl.insert_job(jid, this->earliest_starts[jid]);
	}
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::push_es_forward(bool force_complete,
                                              bool range_changed) noexcept
{
	Timer timer;
	if (this->disaggregate_time) {
		timer.start();
	}

	if (force_complete) {
		this->push_es_forward_queue.insert(
		    this->push_es_forward_queue.end(),
		    this->push_es_forward_out_of_range.begin(),
		    this->push_es_forward_out_of_range.end());
		this->push_es_forward_out_of_range.clear();
	} else if (range_changed) {
		// We need to check which of the unpropagated changes need to be
		// propagated

		size_t i = 0;
		while (i < this->push_es_forward_out_of_range.size()) {
			Job::JobId jid = this->push_es_forward_out_of_range[i];
			if (this->earliest_starts[jid] <= this->active_range.second) {
				this->push_es_forward_queue.push_back(jid);
				std::swap(this->push_es_forward_out_of_range[i],
				          this->push_es_forward_out_of_range.back());
				this->push_es_forward_out_of_range.pop_back();
			} else {
				i++;
			}
		}
	}

	while (!this->push_es_forward_queue.empty()) {
		auto v = this->push_es_forward_queue.back();

		this->push_es_forward_queue.pop_back();

		auto duration = this->durations[v];

		for (const auto & edge : this->adjacency_list[v]) {
			auto new_start = this->earliest_starts[v] + duration;
			if (new_start > this->earliest_starts[edge.t]) {
#ifdef OMG_VERIFY
				const Job & t_job = this->instance.get_job(edge.t);
				assert(t_job.get_deadline() >= new_start + t_job.get_duration());
#endif
				bool can_delay =
				    this->earliest_starts[edge.t] > this->active_range.second;

				this->earliest_starts[edge.t] = new_start;
				if ((!can_delay) || (force_complete)) {
					this->push_es_forward_queue.emplace_back(edge.t);
				} else {
					this->push_es_forward_out_of_range.emplace_back(edge.t);
					//					std::cout << "Deferring " << edge.t << " (now " <<
					// this->push_es_forward_out_of_range.size() << " elements)\n";
				}
				this->changed_nodes_buf.push_back(edge.t);
			}
		}
	}

	if (this->disaggregate_time) {
		this->propagate_time += timer.get();
	}
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::build_candidate_jobs() noexcept
{
	Timer timer;
	if (this->disaggregate_time) {
		timer.start();
	}

	this->candidates_buf.clear();
	// TODO this is way too slow
	for (unsigned int jid = 0; jid < this->job_count; ++jid) {
		if ((this->earliest_starts[jid] <= this->active_range.second) &&
		    (this->earliest_starts[jid] + this->durations[jid] >=
		     this->active_range.first)) {
			this->candidates_buf.push_back(jid);
		}
	}

	this->batch_offset = 0;

	if (this->disaggregate_time) {
		this->job_selection_time += timer.get();
	}
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::build_candidate_edges_batched() noexcept
{
	Timer timer;
	if (this->disaggregate_time) {
		timer.start();
	}

	if (this->randomize_edge_candidates && (this->batch_offset == 0)) {
		std::shuffle(this->candidates_buf.begin(), this->candidates_buf.end(),
		             this->rnd);
	}

	this->score_sum = 0;

	size_t num_jobs = this->candidates_buf.size();
	size_t max_attempt = num_jobs * (num_jobs - 1);
	assert(this->candidate_edge_buf.empty());
	size_t candidate_count = 0;

	while ((this->batch_offset < max_attempt) &&
	       (candidate_count < this->edge_candidate_batchsize)) {
		size_t step = (this->batch_offset / num_jobs) + 1;
		size_t first_job = this->batch_offset % num_jobs;

		Job::JobId s_jid = this->candidates_buf[first_job];
		Job::JobId t_jid = this->candidates_buf[(first_job + step) % num_jobs];

		this->batch_offset++;

		if (this->earliest_starts[t_jid] >=
		    this->earliest_starts[s_jid] + this->durations[s_jid]) {
			// Inserting makes no sense.
			continue;
		}

		// Only try forwards edge
		if (this->latest_finishs[t_jid] >= this->earliest_starts[s_jid] +
		                                       this->durations[s_jid] +
		                                       this->durations[t_jid]) {
			candidate_count++;

			if constexpr (use_mes || use_eps) {
				double score = 1.0;
				if constexpr (use_mes) {
					score += this->mes.get_score_for(s_jid, t_jid);
				}
				if constexpr (use_eps) {
					score += this->eps.get_score_for(s_jid, t_jid);
				}
				this->candidate_edge_buf.emplace_back(score, s_jid, t_jid);
				this->score_sum += score;
			} else {
				this->candidate_edge_buf.emplace_back(s_jid, t_jid);
			}
		}
	}

	if (this->disaggregate_time) {
		this->edge_selection_time += timer.get();
	}
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::build_candidate_edges() noexcept
{
	Timer timer;
	if (this->disaggregate_time) {
		timer.start();
	}

	this->score_sum = 0;

	// sort by start points
	struct cmp
	{
		const std::vector<unsigned int> & start_times;
		cmp(const std::vector<unsigned int> & start_times_in)
		    : start_times(start_times_in)
		{}

		bool
		operator()(unsigned int jid_a, unsigned int jid_b) const noexcept
		{
			return start_times[jid_a] < start_times[jid_b];
		}
	};
	std::sort(this->candidates_buf.begin(), this->candidates_buf.end(),
	          cmp(this->earliest_starts));

	// list of currently active jobs and their end times
	this->active_jobs_buf.clear();

	this->candidate_edge_buf.clear();

	for (Job::JobId jid : this->candidates_buf) {
		unsigned int start = this->earliest_starts[jid];
		// TODO can this be SIMDed?
		for (size_t i = 0; i < this->active_jobs_buf.size();) {
			auto [other_jid, other_end] = this->active_jobs_buf[i];

			if (other_end <= start) {
				// No longer active
				std::swap(this->active_jobs_buf[i], this->active_jobs_buf.back());
				this->active_jobs_buf.pop_back();
			} else {
				// Try forward edge
				if (this->latest_finishs[other_jid] >=
				    start + this->durations[jid] + this->durations[other_jid]) {
					if constexpr (use_mes || use_eps) {
						double score = 1.0;
						if constexpr (use_mes) {
							score += this->mes.get_score_for(jid, other_jid);
						}
						if constexpr (use_eps) {
							score += this->eps.get_score_for(jid, other_jid);
						}
						this->candidate_edge_buf.emplace_back(score, jid, other_jid);
						this->score_sum += score;
					} else {
						this->candidate_edge_buf.emplace_back(jid, other_jid);
					}
				}

				// Try backwards edge
				if (this->latest_finishs[jid] >= this->earliest_starts[other_jid] +
				                                     this->durations[jid] +
				                                     this->durations[other_jid]) {
					if constexpr (use_mes || use_eps) {
						double score = 1.0;
						if constexpr (use_mes) {
							score += this->mes.get_score_for(other_jid, jid);
						}
						if constexpr (use_eps) {
							score += this->eps.get_score_for(other_jid, jid);
						}
						this->candidate_edge_buf.emplace_back(score, other_jid, jid);
						this->score_sum += score;
					} else {
						this->candidate_edge_buf.emplace_back(other_jid, jid);
					}
				}

				++i;
			}
		}

		this->active_jobs_buf.emplace_back(jid, this->earliest_starts[jid] +
		                                            this->durations[jid]);
	}

	if (this->disaggregate_time) {
		this->edge_selection_time += timer.get();
	}
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::reset() noexcept
{
	this->reset_count++;

	Timer reset_timer;
	if (this->disaggregate_time) {
		reset_timer.start();
	}

	double score = this->rsl.get_maximum().getUsage()[0];
	this->solution_count++;

	if constexpr (use_mes) {
		this->mes.incorporate_result(score, this->earliest_starts,
		                             this->adjacency_list);
	}
	if constexpr (use_eps) {
		this->eps.incorporate_result(score, this->earliest_starts,
		                             this->adjacency_list);
	}
	if constexpr (!(use_mes || use_eps)) {
		(void)score;
	}

	for (size_t s = 0; s < this->job_count; ++s) {
		this->adjacency_list[s] = this->base_adjacency_list[s];
		this->rev_adjacency_list[s] = this->base_rev_adjacency_list[s];
	}

	this->earliest_starts = this->base_earliest_starts;
	this->latest_finishs = this->base_latest_finishs;

	// TODO speed this up once Ygg knows how to clone trees
	for (unsigned int jid = 0; jid < this->job_count; ++jid) {
		this->rsl.set_pos(jid, (int)this->earliest_starts[jid]);
	}

	if (this->disaggregate_time) {
		this->reset_time += reset_timer.get();
	}

	this->active_range = this->rsl.get_maximum_range();
	// Propagate for the new active range
	this->iteration_propagate(true, true); // TODO is this necessary?
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::iteration_regenerate_candidates() noexcept
{
	this->build_candidate_jobs();

	this->candidate_edge_buf.clear();

	if (this->edge_candidate_batchsize == 0) {
		this->build_candidate_edges();
	} else {
		this->build_candidate_edges_batched();
	}
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::iteration_propagate(bool complete,
                                                  bool range_changed) noexcept
{
	this->changed_nodes_buf.clear();

	this->push_lf_backward(complete, range_changed);
	this->push_es_forward(complete, range_changed);

	this->node_moved_buf.reset(); // TODO does this even speed things up?

	Timer timer;
	if (this->disaggregate_time) {
		timer.start();
	}

	for (auto jid : this->changed_nodes_buf) {
		if (!this->node_moved_buf[jid]) {
			this->rsl.set_pos(jid, (int)this->earliest_starts[jid]);
			this->node_moved_buf[jid] = true;
		}
	}

	if (this->disaggregate_time) {
		this->skyline_update_time += timer.get();
	}
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::iteration_unstick() noexcept
{
	Timer timer;
	if (this->disaggregate_time) {
		timer.start();
	}

	this->iteration_propagate(true, false);

	if (this->rsl.get_maximum_range() != this->active_range) {
		this->active_range = this->rsl.get_maximum_range();
		// We just propagated everything, so we don't need to do it again here.
		// this->dbg_verify_correctly_partially_propagated();

		// We got a different peak! Maybe we can do something there…
		this->iteration_regenerate_candidates();
		if (!this->candidate_edge_buf.empty()) {
			// Ha, that worked!
			if (disaggregate_time) {
				this->unstick_time += timer.get();
			}
			return;
		}
	}

	// We are going to do something that potentially decreases solution quality -
	// check if we saw a new best solution!
	if (this->rsl.get_maximum().getUsage()[0] < this->best_score) {
		this->best_score = this->rsl.get_maximum().getUsage()[0];
		for (Job::JobId jid = 0; jid < this->job_count; ++jid) {
			this->best_start_times[jid] = this->earliest_starts[jid];
		}
	}

	// Okay, now for actual unsticking. If we can still delete…
	if (this->deletions_remaining > 0) {
		// Then delete!
		this->deletions_remaining--;

		Timer deletion_timer;
		if (this->disaggregate_time) {
			deletion_timer.start();
		}

		this->create_new_candidate_edges();

		if (this->candidate_edge_buf.empty()) {
			//			BOOST_LOG(l.d(3)) << ">>> No success, resetting.";
			this->reset();
			this->iteration_regenerate_candidates();
			this->deletions_remaining = this->deletions_before_reset;
			if (disaggregate_time) {
				this->unstick_time += timer.get();
			}
			return;
		} else {
			//			BOOST_LOG(l.d(3)) << ">>> Success!";
		}
	} else {
		// No possibility found
		//	BOOST_LOG(l.d(5)) << ">> Resetting!";

		this->reset();
		this->iteration_regenerate_candidates();
		this->deletions_remaining = this->deletions_before_reset;
		if (disaggregate_time) {
			this->unstick_time += timer.get();
		}
		return;
	}

	if (disaggregate_time) {
		this->unstick_time += timer.get();
	}
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::iteration() noexcept
{
	this->dbg_verify();

	this->iteration_count++;

	bool active_range_changed = false;

	/* Bookkeeping */
	// Logging
	if (((this->iteration_count % 10000) == 0) && (this->log_timer.get() > 5)) {
		double time = this->run_timer.get() - this->last_log_time;
		BOOST_LOG(l.i()) << "Iterations per second: "
		                 << (double)(this->iteration_count -
		                             this->last_log_iteration) /
		                        time
		                 << "\t Best Score: " << best_score;

		BOOST_LOG(l.i()) << "  > Deletions: " << this->deletion_count << "\t"
		                 << " > Resets: " << this->reset_count;
		this->last_log_time = this->run_timer.get();
		this->last_log_iteration = this->iteration_count;
		this->log_timer.start();
	}

	/* Write intermediate scores if requested */
	if (__builtin_expect(this->intermediate_score_interval > 0, 0)) {
		double time = this->run_timer.get();
		if ((time - this->intermediate_score_last_time) >
		    this->intermediate_score_interval) {
			this->intermediate_score_last_time = time;
			this->additional.intermediate_results.push_back(
			    {{time},
			     {(unsigned int)this->iteration_count},
			     {this->best_score},
			     {},
			     {}});
		}
	}

	// Have the scorers do bookkeeping
	if constexpr (use_mes) {
		this->mes.iteration(this->iteration_count);
	}
	if constexpr (use_eps) {
		this->eps.iteration(this->iteration_count);
	}

	/* End of bookkeeping */
	// Periodically push everything through
	if ((this->iteration_count - this->last_complete_push) >=
	    this->force_complete_push_after) {
		this->push_es_forward(true, true);
		this->push_lf_backward(true, true);
	}

	// Periodically check if our active range is still okay
	if ((this->iteration_count - this->last_range_check) >=
	    this->force_range_check_after) {
		this->last_range_check = this->iteration_count;

		auto max_range = this->rsl.get_maximum_range();
		if (max_range != this->active_range) {
			// Active range has changed
			active_range_changed = true;

			// Clear everything.
			this->candidates_buf.clear();
			this->candidate_edge_buf.clear();
			this->active_range = max_range;

			// Propagate for the new active range
			this->iteration_propagate(true, true); // TODO is complete necessary?
		}
	}

	// No more candidates. We need new ones.
	if (this->candidate_edge_buf.empty()) {
		if ((active_range_changed) || (this->iteration_count == 1)) {
			// It's fine - just generate new candidates.
			this->iteration_regenerate_candidates();
			if (this->candidate_edge_buf.empty()) {
				// Uh-oh. We're really stuck. Try to unstick by deleting or resetting.
				this->iteration_unstick();
			}
		} else {
			// We're still in the same place. In the case of batched generation,
			// that could mean we just need to generate the next batch. Otherwise,
			// we need to unstick.
			if (this->edge_candidate_batchsize > 0) {
				this->build_candidate_edges_batched();
				if (this->candidate_edge_buf.empty()) {
					// Still no luck - now we need to unstick.
					this->iteration_unstick();
				}
			} else {
				this->iteration_unstick();
			}
		}
	}

	// At this point, we should have a candidate for insertion.
	assert(!this->candidate_edge_buf.empty());

	bool inserted = false;
	// Thus, insert one edge.
	if ((this->iteration_count - this->last_complete_push) >=
	    this->force_complete_push_after) {
		// Loop until we could actually insert an edge
		while (!inserted && (!this->candidate_edge_buf.empty())) {
			inserted = this->iteration_insert_edge(true);
		}
		this->last_complete_push =
		    this->iteration_count + 1; // Already done for the next iteration…
	} else {
		// Loop until we could actually insert an edge
		while (!inserted && (!this->candidate_edge_buf.empty())) {
			inserted = this->iteration_insert_edge(false);
		}
	}
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::push_lf_backward(bool force_complete,
                                               bool range_changed) noexcept
{
	Timer timer;
	if (this->disaggregate_time) {
		timer.start();
	}

	if (force_complete) {
		this->push_lf_backward_queue.insert(
		    this->push_lf_backward_queue.end(),
		    this->push_lf_backward_out_of_range.begin(),
		    this->push_lf_backward_out_of_range.end());
		this->push_lf_backward_out_of_range.clear();
	} else if (range_changed) {
		// We need to check which of the unpropagated changes need to be
		// propagated

		size_t i = 0;

		while (i < this->push_lf_backward_out_of_range.size()) {
			Job::JobId jid = this->push_lf_backward_out_of_range[i];
			if (this->latest_finishs[jid] >= this->active_range.first) {
				this->push_lf_backward_queue.push_back(jid);
				std::swap(this->push_lf_backward_out_of_range[i],
				          this->push_lf_backward_out_of_range.back());
				this->push_lf_backward_out_of_range.pop_back();
			} else {
				i++;
			}
		}
	}

	while (!this->push_lf_backward_queue.empty()) {
		auto v = this->push_lf_backward_queue.back();
		this->push_lf_backward_queue.pop_back();

		auto duration = this->durations[v];

		auto new_finish = this->latest_finishs[v] - duration;

		for (const auto & rev_edge : this->rev_adjacency_list[v]) {
			bool can_delay =
			    this->latest_finishs[rev_edge.s] < this->active_range.first;

			if (new_finish < this->latest_finishs[rev_edge.s]) {
				this->latest_finishs[rev_edge.s] = new_finish;
				if ((!can_delay) || (force_complete)) {
					this->push_lf_backward_queue.emplace_back(rev_edge.s);
				} else {
					this->push_lf_backward_out_of_range.emplace_back(rev_edge.s);
				}
			}
		}
	}

	if (this->disaggregate_time) {
		this->propagate_time += timer.get();
	}
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::rebuild_lf_backward() noexcept
{
	while (!this->rebuild_lf_backward_queue.empty()) {
		auto v = this->rebuild_lf_backward_queue.back();

		this->rebuild_lf_backward_queue.pop_back();

		unsigned int min_seen = this->deadlines[v];

		for (const auto & edge : this->adjacency_list[v]) {
			min_seen = std::min(min_seen, this->latest_finishs[edge.t] -
			                                  this->durations[edge.t]);
		}
		if (min_seen > this->latest_finishs[v]) {
			this->latest_finishs[v] = min_seen;
			for (const auto & rev_edge : this->rev_adjacency_list[v]) {
				this->rebuild_lf_backward_queue.emplace_back(rev_edge.s);
			}
		}
	}
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::rebuild_es_forward() noexcept
{
	while (!this->rebuild_es_forward_queue.empty()) {
		auto v = this->rebuild_es_forward_queue.back();
		this->rebuild_es_forward_queue.pop_back();

		unsigned int max_seen = this->releases[v];

		for (const auto & rev_edge : this->rev_adjacency_list[v]) {
			max_seen = std::max(max_seen, this->earliest_starts[rev_edge.s] +
			                                  this->durations[rev_edge.s]);
		}
		if (max_seen < this->earliest_starts[v]) {
			this->earliest_starts[v] = max_seen;
			for (const auto & edge : this->adjacency_list[v]) {
				this->rebuild_es_forward_queue.emplace_back(edge.t);
			}
			this->changed_nodes_buf.push_back(v);
		}
	}
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::insert_edge(Job::JobId s, Job::JobId t,
                                          bool force_complete) noexcept
{
	this->insertion_count++;

	this->graph_insert_edge(s, t);

	this->changed_nodes_buf.clear();
	this->push_es_forward_queue.clear(); // TODO should be unnecessary
	this->push_es_forward_queue.emplace_back(s);
	this->push_es_forward(force_complete, false);
	this->push_lf_backward_queue.clear(); // TODO should be unnecessary
	this->push_lf_backward_queue.emplace_back(t);
	this->push_lf_backward(force_complete, false);

	this->node_moved_buf.reset();
	Timer skyline_timer;
	if (this->disaggregate_time) {
		skyline_timer.start();
	}
	for (auto jid : this->changed_nodes_buf) {
		if (!this->node_moved_buf[jid]) {
			this->rsl.set_pos(jid, (int)this->earliest_starts[jid]);
			this->node_moved_buf[jid] = true;
		}
	}
	if (this->disaggregate_time) {
		this->skyline_update_time += skyline_timer.get();
	}
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::delete_edge(Job::JobId s,
                                          size_t s_adj_list_index) noexcept
{
	Job::JobId t = this->adjacency_list[s][s_adj_list_index];
	this->graph_delete_edge(s, s_adj_list_index);
	Timer update_timer;
	if (this->disaggregate_time) {
		update_timer.start();
	}

	this->changed_nodes_buf.clear();
	this->rebuild_es_forward_buf.push_back(t);
	this->rebuild_es_forward();
	this->rebuild_lf_backward_buf.push_back(s);
	this->rebuild_lf_backward();

	if (this->disaggregate_time) {
		this->time_update_time += update_timer.get();
	}

	this->node_moved_buf.reset();
	Timer skyline_timer;
	if (this->disaggregate_time) {
		skyline_timer.start();
	}
	for (auto jid : this->changed_nodes_buf) {
		if (!this->node_moved_buf[jid]) {
			this->rsl.set_pos(jid, this->earliest_starts[jid]);
			this->node_moved_buf[jid] = true;
		}
	}
	if (this->disaggregate_time) {
		this->skyline_update_time += skyline_timer.get();
	}
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::run()
{
	this->deletions_remaining = this->deletions_before_reset;

	if constexpr (use_mes) {
		BOOST_LOG(l.d(2)) << "Using MES scoring";
	}
	if constexpr (use_eps) {
		BOOST_LOG(l.d(2)) << "Using EPS scoring";
	}

	this->run_timer.start();
	this->log_timer.start();

	this->iteration_count = 0;
	this->insertion_count = 0;

	BOOST_LOG(l.d(3)) << "Initializing graph...";
	this->initialize_graph();
	BOOST_LOG(l.d(3)) << "Initializing times...";
	this->initialize_times();
	BOOST_LOG(l.d(3)) << "Initializing skyline...";
	this->initialize_skyline();
	this->active_range = this->rsl.get_maximum_range();

	BOOST_LOG(l.d(2)) << "Initialization done.";

	while (this->run_timer.get() < this->timelimit) {
		this->iteration();
	}

	double elapsed_time = this->run_timer.get();
	this->additional.extended_measures.push_back(
	    {"ITERATIONS_PER_SECOND",
	     {},
	     {},
	     AdditionalResultStorage::ExtendedMeasure::TYPE_DOUBLE,
	     {(double)this->iteration_count / elapsed_time}});
	this->additional.extended_measures.push_back(
	    {"ITERATION_COUNT",
	     {},
	     {},
	     AdditionalResultStorage::ExtendedMeasure::TYPE_INT,
	     {(double)this->iteration_count}});
	this->additional.extended_measures.push_back(
	    {"RESET_COUNT",
	     {},
	     {},
	     AdditionalResultStorage::ExtendedMeasure::TYPE_INT,
	     {(double)this->reset_count}});
	this->additional.extended_measures.push_back(
	    {"INSERTION_COUNT",
	     {},
	     {},
	     AdditionalResultStorage::ExtendedMeasure::TYPE_INT,
	     {(double)this->insertion_count}});
	this->additional.extended_measures.push_back(
	    {"DELETION_COUNT",
	     {},
	     {},
	     AdditionalResultStorage::ExtendedMeasure::TYPE_INT,
	     {(double)this->deletion_count}});
	this->additional.extended_measures.push_back(
	    {"SOLUTION_COUNT",
	     {},
	     {},
	     AdditionalResultStorage::ExtendedMeasure::TYPE_INT,
	     {(double)this->solution_count}});
	if (this->disaggregate_time) {
		this->additional.extended_measures.push_back(
		    {"SKYLINE_UPDATE_TIME",
		     {},
		     {},
		     AdditionalResultStorage::ExtendedMeasure::TYPE_DOUBLE,
		     {this->skyline_update_time}});
		this->additional.extended_measures.push_back(
		    {"PROPAGATE_TIME",
		     {},
		     {},
		     AdditionalResultStorage::ExtendedMeasure::TYPE_DOUBLE,
		     {this->propagate_time}});
		this->additional.extended_measures.push_back(
		    {"RESET_TIME",
		     {},
		     {},
		     AdditionalResultStorage::ExtendedMeasure::TYPE_DOUBLE,
		     {this->reset_time}});
		this->additional.extended_measures.push_back(
		    {"SELECTION_TIME",
		     {},
		     {},
		     AdditionalResultStorage::ExtendedMeasure::TYPE_DOUBLE,
		     {this->edge_selection_time + this->job_selection_time}});
		this->additional.extended_measures.push_back(
		    {"EDGE_SELECTION_TIME",
		     {},
		     {},
		     AdditionalResultStorage::ExtendedMeasure::TYPE_DOUBLE,
		     {this->edge_selection_time}});
		this->additional.extended_measures.push_back(
		    {"JOB_SELECTION_TIME",
		     {},
		     {},
		     AdditionalResultStorage::ExtendedMeasure::TYPE_DOUBLE,
		     {this->job_selection_time}});
		this->additional.extended_measures.push_back(
		    {"UNSTICK_TIME",
		     {},
		     {},
		     AdditionalResultStorage::ExtendedMeasure::TYPE_DOUBLE,
		     {this->unstick_time}});
	}
}

template <bool use_mes, bool use_eps>
bool
SWAGSolver<use_mes, use_eps>::iteration_insert_edge(
    bool force_complete) noexcept
{
	size_t i = 0;

	if constexpr (use_mes || use_eps) {
		std::uniform_real_distribution<double> distr(0, this->score_sum);
		double point = distr(this->rnd);
		double sum = 0;
		while ((i < this->candidate_edge_buf.size() - 1) &&
		       (sum + std::get<0>(this->candidate_edge_buf.at(i)) < point)) {
			sum += std::get<0>(this->candidate_edge_buf.at(i));
			i++;
		}
	} else {
		std::uniform_int_distribution<size_t> distr(
		    0, this->candidate_edge_buf.size() - 1);
		i = distr(this->rnd);
	}

	Job::JobId s;
	Job::JobId t;

	if constexpr (use_mes || use_eps) {
		s = std::get<1>(this->candidate_edge_buf.at(i));
		t = std::get<2>(this->candidate_edge_buf.at(i));
	} else {
		s = this->candidate_edge_buf.at(i).first;
		t = this->candidate_edge_buf.at(i).second;
	}

	bool inserted = false;
	// Make sure they still overlap
	if ((this->earliest_starts[s] <
	     this->earliest_starts[t] + this->durations[t]) &&
	    (this->earliest_starts[t] <
	     this->earliest_starts[s] + this->durations[s])) {

		// Make sure both jobs are still within the active range
		if ((this->earliest_starts[s] + this->durations[s] >=
		     this->active_range.first) &&
		    (this->earliest_starts[s] <= this->active_range.second) &&
		    (this->earliest_starts[t] + this->durations[t] >=
		     this->active_range.first) &&
		    (this->earliest_starts[t] <= this->active_range.second)) {
			// Make sure windows still fit
			// At this point it is *very* important that everything is propagated
			// properly!
			if (this->earliest_starts[s] + this->durations[s] + this->durations[t] <=
			    this->latest_finishs[t]) {
				this->insert_edge(s, t, force_complete);
				inserted = true;

			} // else: too tight
		}   // else: not in active range
	}     // else: not overlapping anymore

	// Remove edge from candidates
	if constexpr (use_mes || use_eps) {
		auto edge_score = std::get<0>(this->candidate_edge_buf.at(i));
		this->score_sum -= edge_score;
	}
	std::swap(this->candidate_edge_buf.at(i), this->candidate_edge_buf.back());
	this->candidate_edge_buf.pop_back();

	return inserted;
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::dbg_print_adjacencies()
{
	std::cout << "////// Adjacencies ///////\n";
	std::cout << "Forward:\n";
	for (Job::JobId jid = 0; jid < this->job_count; ++jid) {
		if (!this->adjacency_list[jid].empty()) {
			std::cout << "  " << jid << ": ";
			for (const auto & edge : this->adjacency_list[jid]) {
				std::cout << "(" << edge.t << "/" << edge.rev_index << ") ";
			}
			std::cout << "\n";
		}
	}
	std::cout << "Reverse:\n";
	for (Job::JobId jid = 0; jid < this->job_count; ++jid) {
		if (!this->rev_adjacency_list[jid].empty()) {
			std::cout << "  " << jid << ": ";
			for (const auto & rev_edge : this->rev_adjacency_list[jid]) {
				std::cout << "(" << rev_edge.s << "/" << rev_edge.forward_index << ")";
			}
			std::cout << "\n";
		}
	}
	std::cout << "//////////////////////////\n";
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::dbg_print_graph()
{
	std::ostringstream buf;
	this->dbg_generate_dot(buf);

	std::cout << buf.str();
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::dbg_write_graph(std::string filename)
{
	std::ostringstream buf;
	this->dbg_generate_dot(buf);

	std::ofstream file;
	file.open(filename);
	file << buf.str();
	file.close();
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::dbg_generate_dot(std::ostringstream & buf)
{
	buf << "digraph G {\n";
	for (Job::JobId jid = 0; jid < this->instance.job_count(); ++jid) {
		const Job & job = this->instance.get_job(jid);
		buf << "  " << jid << " [label=\"" << jid << " / " << job.get_duration()
		    << " @ " << job.get_release() << " -> " << job.get_deadline() << " ("
		    << this->earliest_starts[jid] << ")"
		    << "\"];\n";
	}
	buf << "\n\n\n";

	for (Job::JobId s = 0; s < this->instance.job_count(); ++s) {
		for (const auto & e : this->adjacency_list[s]) {
			Job::JobId t = e.t;

			buf << "  " << s << " -> " << t;
			if (e.is_permanent()) {
				buf << " [label=\"p\"]";
			}
			buf << ";\n";
		}
	}

	buf << "}\n";
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::dbg_verify()
{
#ifdef OMG_VERIFY
	this->dbg_verify_graph();
	this->dbg_verify_solution() t;
	this->dbg_verify_times();
	this->dbg_verify_cycle_free();
#endif
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::dbg_verify_active_range()
{
#ifdef OMG_VERIFY
	for (Job::JobId job_cand : this->candidates_buf) {
		assert(this->earliest_starts[job_cand] <= this->active_range.second);
		assert(this->earliest_starts[job_cand] + this->durations[job_cand] >=
		       this->active_range.first);
	}

	for (auto & edge_cand : this->candidate_edge_buf) {
		Job::JobId s;
		Job::JobId t;
		if constexpr (use_mes || use_eps) {
			s = std::get<1>(edge_cand);
			t = std::get<2>(edge_cand);
		} else {
			s = edge_cand.first;
			t = edge_cand.second;
		}

		assert(this->earliest_starts[s] <= this->active_range.second);
		assert(this->earliest_starts[s] + this->durations[s] >=
		       this->active_range.first);
		assert(this->earliest_starts[t] <= this->active_range.second);
		assert(this->earliest_starts[t] + this->durations[t] >=
		       this->active_range.first);
	}
#endif
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::dbg_verify_values_during_forwards_bfs()
{
#ifdef OMG_VERIFY
	std::vector<unsigned int> lfs(this->job_count);
	std::deque<Job::JobId> queue;

	for (Job::JobId jid = 0; jid < this->job_count; ++jid) {
		lfs[jid] = this->base_latest_finishs[jid];
		queue.push_back(jid);

		if (this->edgedel_vertex_seen[jid]) {
			assert(this->edgedel_current_value[jid] >=
			       this->base_earliest_starts[jid] + this->durations[jid]);
			assert(this->edgedel_current_value[jid] <=
			       this->base_latest_finishs[jid]);
		}
	}

	while (!queue.empty()) {
		Job::JobId t = queue[0];
		queue.pop_front();

		for (auto & rev_edge : this->rev_adjacency_list[t]) {
			Edge & e = this->adjacency_list[rev_edge.s][rev_edge.forward_index];

			if (e.is_marked()) {
				// Deleted - ignore!
				continue;
			}

			assert(lfs[t] >= this->durations[t]);
			auto new_lf = lfs[t] - this->durations[t];
			if (new_lf < lfs[rev_edge.s]) {
				lfs[rev_edge.s] = lfs[t] - this->durations[t];
				queue.push_back(rev_edge.s);
			}
		}
	}

	for (Job::JobId jid = 0; jid < this->job_count; ++jid) {
		if (this->edgedel_vertex_seen[jid]) {
			assert(lfs[jid] >= this->edgedel_current_value[jid]);
		}
	}
#endif
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::dbg_verify_values_during_backwards_bfs()
{
#ifdef OMG_VERIFY
	std::vector<unsigned int> ess(this->job_count);
	std::deque<Job::JobId> queue;

	for (Job::JobId jid = 0; jid < this->job_count; ++jid) {
		ess[jid] = this->base_earliest_starts[jid];
		queue.push_back(jid);
	}

	while (!queue.empty()) {
		Job::JobId s = queue[0];
		queue.pop_front();

		for (auto & e : this->adjacency_list[s]) {

			if (e.is_marked()) {
				// Deleted - ignore!
				continue;
			}

			auto new_es = ess[s] + this->durations[s];
			if (new_es > ess[e.t]) {
				ess[e.t] = new_es;
				queue.push_back(e.t);
			}
		}
	}

	for (Job::JobId jid = 0; jid < this->job_count; ++jid) {
		if (this->edgedel_vertex_seen[jid]) {
			assert(ess[jid] <= this->edgedel_current_value[jid]);
		}
	}
#endif
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::dbg_verify_limits_during_partial_propagation()
{
#ifdef OMG_VERIFY
	std::vector<unsigned int> ess(this->job_count);
	std::deque<Job::JobId> queue;

	for (Job::JobId jid = 0; jid < this->job_count; ++jid) {
		ess[jid] = this->base_earliest_starts[jid];
		queue.push_back(jid);
	}

	while (!queue.empty()) {
		Job::JobId s = queue[0];
		queue.pop_front();

		for (auto & e : this->adjacency_list[s]) {
			auto new_es = ess[s] + this->durations[s];
			if (new_es > ess[e.t]) {
				ess[e.t] = new_es;
				queue.push_back(e.t);
			}
		}
	}

	for (Job::JobId jid = 0; jid < this->job_count; ++jid) {
		assert(ess[jid] >= this->releases[jid]);

		if (!(ess[jid] + this->durations[jid] <= this->deadlines[jid])) {
			this->dbg_print_graph();
		}
		assert(ess[jid] + this->durations[jid] <= this->deadlines[jid]);
	}
#endif
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::dbg_verify_correctly_partially_propagated()
{
#ifdef OMG_VERIFY
	std::vector<unsigned int> ess(this->job_count);
	std::vector<unsigned int> lfs(this->job_count);
	std::deque<Job::JobId> queue;

	for (Job::JobId jid = 0; jid < this->job_count; ++jid) {
		ess[jid] = this->base_earliest_starts[jid];
		lfs[jid] = this->base_latest_finishs[jid];
		queue.push_back(jid);
	}

	// Compute earliest starts
	while (!queue.empty()) {
		Job::JobId s = queue[0];
		queue.pop_front();

		for (auto & e : this->adjacency_list[s]) {
			auto new_es = ess[s] + this->durations[s];
			if (new_es > ess[e.t]) {
				ess[e.t] = new_es;
				queue.push_back(e.t);
			}
		}
	}
	std::cout << std::flush;

	// Compute latest finishs
	for (Job::JobId jid = 0; jid < this->job_count; ++jid) {
		queue.push_back(jid);
	}
	while (!queue.empty()) {
		Job::JobId t = queue[0];
		queue.pop_front();

		for (auto & rev_e : this->rev_adjacency_list[t]) {
			auto new_lf = lfs[t] - this->durations[t];
			if (new_lf < lfs[rev_e.s]) {
				lfs[rev_e.s] = new_lf;
				queue.push_back(rev_e.s);
			}
		}
	}

	for (Job::JobId jid = 0; jid < this->job_count; ++jid) {
		if (ess[jid] <= this->active_range.second) {
			assert(ess[jid] == this->earliest_starts[jid]);
		}

		if (lfs[jid] >= this->active_range.first) {
			assert(lfs[jid] == this->latest_finishs[jid]);
		}
	}

	// Everything that is a potential candidate must be correctly propagated!
	if constexpr (use_mes || use_eps) {
		for (auto [score, s, t] : this->candidate_edge_buf) {
			assert(ess[s] == this->earliest_starts[s]);
			assert(lfs[t] == this->latest_finishs[t]);
		}
	} else {
		for (auto [s, t] : this->candidate_edge_buf) {
			assert(ess[s] == this->earliest_starts[s]);
			assert(lfs[t] == this->latest_finishs[t]);
		}
	}
#endif
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::dbg_verify_times()
{
#ifdef OMG_VERIFY
	for (unsigned int jid = 0; jid < this->job_count; ++jid) {
		const auto & job = this->instance.get_job(jid);

		unsigned int start_time = job.get_release();
		unsigned int latest_finish = job.get_deadline();

		for (const auto & rev_edge : this->rev_adjacency_list[jid]) {
			start_time = std::max(
			    start_time, this->earliest_starts[rev_edge.s] +
			                    this->instance.get_job(rev_edge.s).get_duration());
		}

		for (const auto & edge : this->adjacency_list[jid]) {
			latest_finish = std::min(
			    latest_finish, this->latest_finishs[edge.t] -
			                       this->instance.get_job(edge.t).get_duration());
		}

		if (start_time <= this->active_range.second) {
			assert(start_time == this->earliest_starts[jid]);
		}

		if (start_time >= this->active_range.first) {
			assert(latest_finish == this->latest_finishs[jid]);
		}
	}
#endif
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::dbg_verify_solution()
{
#ifdef OMG_VERIFY
	Solution sol(this->instance, false, this->earliest_starts, {});
	assert(sol.get_max_usage(0) >= this->rsl.get_maximum().getUsage()[0] * 0.999);
	assert(sol.get_max_usage(0) <= this->rsl.get_maximum().getUsage()[0] * 1.001);
	assert(sol.is_feasible());
#endif
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::dbg_verify_cycle_free()
{
#ifdef OMG_VERIFY
	std::vector<bool> seen(this->job_count, false);
	std::vector<bool> active(this->job_count, false);
	struct StackEntry
	{
		bool backtrack;
		size_t v;
	};
	std::vector<StackEntry> stack;
	std::vector<size_t> recursion_stack;

	for (Job::JobId jid = 0; jid < this->job_count; ++jid) {
		if (seen[jid]) {
			continue;
		}

		stack.push_back({false, jid});
		active[jid] = true;
		recursion_stack.push_back(jid);

		while (!stack.empty()) {
			auto [backtrack, v] = stack.back();
			stack.pop_back();
			seen[v] = true;

			if (backtrack) {
				active[v] = false;
				assert(recursion_stack.back() == v);
				recursion_stack.pop_back();
			} else {
				stack.push_back({true, v});
				active[v] = true;
				recursion_stack.push_back(v);

				for (auto e : this->adjacency_list[v]) {
					if (!seen[e.t]) {
						assert(!active[e.t]); // This means cycle!
						stack.push_back({false, e.t});
					}
				}
			}
		}
	}

#endif
}

template <bool use_mes, bool use_eps>
void
SWAGSolver<use_mes, use_eps>::dbg_verify_graph()
{
#ifdef OMG_VERIFY
	size_t edge_count = 0;
	for (unsigned int s = 0; s < this->job_count; ++s) {
		std::set<Job::JobId>
		    seen_targets; // for making sure that every edge is added only once
		for (size_t index = 0; index < this->adjacency_list[s].size(); ++index) {
			const auto & edge = this->adjacency_list[s][index];
			unsigned int t = edge.t;
			assert(seen_targets.find(t) == seen_targets.end());
			seen_targets.insert(t);

			// Check that the correct reverse edge is there
			assert(edge.rev_index < this->rev_adjacency_list[t].size());
			const auto & rev_edge = this->rev_adjacency_list[t][edge.rev_index];
			assert(rev_edge.s == s);
			assert(rev_edge.forward_index == index);
		}
		edge_count += this->adjacency_list[s].size();
	}
	assert(edge_count < (this->job_count * this->job_count));

	size_t rev_edge_count = 0;
	for (unsigned int t = 0; t < this->job_count; ++t) {
		for (size_t index = 0; index < this->rev_adjacency_list[t].size();
		     ++index) {
			const auto & rev_edge = this->rev_adjacency_list[t][index];
			// Check that everything in the reverse adjacency list is also in the
			// adj.list
			assert(rev_edge.forward_index < this->adjacency_list[rev_edge.s].size());
			const auto & edge =
			    this->adjacency_list[rev_edge.s][rev_edge.forward_index];
			assert(edge.t == t);
		}
		rev_edge_count += this->rev_adjacency_list[t].size();
	}

	assert(rev_edge_count == edge_count);
	// std::cout << "Edge count: " << edge_count << "\n";

#endif
}

template <bool use_mes, bool use_eps>
Solution
SWAGSolver<use_mes, use_eps>::get_solution()
{
	return Solution(this->instance, false, this->best_start_times, {});
}

} // namespace detail

/**********************************
 * Dispatcher
 **********************************/
SWAGSolver::SWAGSolver(const Instance & instance,
                       AdditionalResultStorage & additional,
                       const SolverConfig & sconf)
    : impl{detail::SWAGSolver<false, false>(instance, additional, sconf),
           detail::SWAGSolver<false, true>(instance, additional, sconf),
           detail::SWAGSolver<true, false>(instance, additional, sconf),
           detail::SWAGSolver<true, true>(instance, additional, sconf)}

{
	this->impl_index = 0;
	if (sconf.has_config("use_mes") && sconf["use_mes"]) {
		this->impl_index += 2;
	}
	if (sconf.has_config("use_eps") && sconf["use_eps"]) {
		this->impl_index += 1;
	}
}

void
SWAGSolver::run()
{
	switch (this->impl_index) {
	case 0:
		std::get<0>(this->impl).run();
		break;
	case 1:
		std::get<1>(this->impl).run();
		break;
	case 2:
		std::get<2>(this->impl).run();
		break;
	case 3:
		std::get<3>(this->impl).run();
		break;
	default:
		assert(false);
	}
}

Solution
SWAGSolver::get_solution()
{
	switch (this->impl_index) {
	case 0:
		return std::get<0>(this->impl).get_solution();
	case 1:
		return std::get<1>(this->impl).get_solution();
	case 2:
		return std::get<2>(this->impl).get_solution();
	case 3:
		return std::get<3>(this->impl).get_solution();
	default:
		assert(false);
		// Just here to remove the warning
		return std::get<0>(this->impl).get_solution();
	}
}

std::string
SWAGSolver::get_id()
{
	return "SWAG v.1.0";
}

Maybe<double>
SWAGSolver::get_lower_bound()
{
	return {};
}

const Traits &
SWAGSolver::get_requirements()
{
	return SWAGSolver::required_traits;
}

const Traits SWAGSolver::required_traits =
    Traits(Traits::LAGS_ONLY_SUCCESSORS | Traits::NO_DRAIN |
               Traits::NO_WINDOW_EXTENSION | Traits::ZERO_AVAILABILITY,
           1, {0.0}, {0.0, 1.0});

} // namespace swag
