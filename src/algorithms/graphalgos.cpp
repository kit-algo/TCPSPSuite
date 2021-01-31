#include "graphalgos.hpp"

#include "../manager/errors.hpp"   // for InconsistentDataError
#include "../util/fault_codes.hpp" // for FAULT_CRITICAL_PATH_INFEASIBLE
#include "datastructures/fast_reset_vector.hpp"
#include "instance/instance.hpp" // for Instance
#include "instance/job.hpp"      // for Job
#include "instance/laggraph.hpp" // for LagGraph, LagGraph::(anonymous)

#include <algorithm>           // for move, max, reverse
#include <assert.h>            // for assert
#include <bits/std_function.h> // for function
#include <boost/range/adaptor/reversed.hpp>
#include <ext/alloc_traits.h> // for __alloc_traits<>::value_type
#include <functional>         // for function
#include <numeric>
#include <string> // for string
#include <unordered_set>

NecessaryOrderComputer::NecessaryOrderComputer(const Instance & instance_in)
    : predecessor_count(instance_in.job_count(), 0),
      successor_count(instance_in.job_count(), 0), instance(instance_in)
{
	CriticalPathComputer cp(this->instance);
	this->earliest_starts = cp.get_forward();
	this->latest_finishs = cp.get_reverse();
	this->compute();
}

void
NecessaryOrderComputer::compute()
{
	std::vector<unsigned int> by_es(this->instance.job_count());
	std::vector<unsigned int> by_lf(this->instance.job_count());

	std::iota(by_es.begin(), by_es.end(), 0);
	std::iota(by_lf.begin(), by_lf.end(), 0);

	std::sort(by_es.begin(), by_es.end(),
	          [&](unsigned int lhs, unsigned int rhs) {
		          return this->earliest_starts[lhs] < this->earliest_starts[rhs];
	          });
	std::sort(by_lf.begin(), by_lf.end(),
	          [&](unsigned int lhs, unsigned int rhs) {
		          return this->latest_finishs[lhs] < this->latest_finishs[rhs];
	          });

	size_t finished = 0;
	for (const auto & v : by_es) {
		while (this->latest_finishs[by_lf[finished]] <= this->earliest_starts[v]) {
			finished++;
		}
		this->predecessor_count[v] = finished;
	}

	size_t started = 0;
	auto start_it = by_es.rbegin();
	for (auto it = by_lf.rbegin(); it != by_lf.rend(); it++) {
		while (this->earliest_starts[*start_it] >= this->latest_finishs[*it]) {
			started++;
			start_it++;
		}

		this->successor_count[*it] = started;
	}
}

const std::vector<size_t> &
NecessaryOrderComputer::get_successor_count() const noexcept
{
	return this->successor_count;
}

const std::vector<size_t> &
NecessaryOrderComputer::get_predecessor_count() const noexcept
{
	return this->predecessor_count;
}

TopologicalSort::TopologicalSort(const LagGraph & graph_in) : graph(graph_in) {}

std::vector<LagGraph::vertex>
TopologicalSort::get()
{
	using vertex = LagGraph::vertex;

	std::vector<unsigned int> indices(this->graph.vertex_count(), 0);

	unsigned int index = (unsigned int)(this->graph.vertex_count() - 1);

	std::function<bool(vertex, vertex)> visit = [&](vertex v, vertex from) {
		(void)from;
		(void)v;
		return true;
	};
	std::function<void(vertex, vertex, int)> traverse = [&](vertex from,
	                                                        vertex to, int lag) {
		(void)from;
		(void)to;
		(void)lag;
	};
	std::function<void(vertex)> backtrack = [&](unsigned int v) {
		if (indices[v] <= index) {
			indices[v] = index;
			index--;
		}
	};

#ifdef ASSERTIONS
	bool finished = false;
#endif

	std::vector<bool> visited;

	for (vertex v = 0; v < this->graph.vertex_count(); ++v) {
		if (this->graph.reverse_neighbor_count(v) > 0) {
			continue; // Only select vertices without incoming edges as roots
		}
		assert(indices[v] == 0);
		DFS<decltype(visit), decltype(backtrack), decltype(traverse)>(
		    this->graph, v, visit, backtrack, traverse);

#ifdef ASSERTIONS
		if (index == ((unsigned int)0 - 1)) {
			finished = true;
		}
#endif
	}

#ifdef ASSERTIONS
	assert(finished);
#endif

	std::vector<vertex> ordered(this->graph.vertex_count());
	for (vertex v = 0; v < this->graph.vertex_count(); ++v) {
		ordered[indices[v]] = v;
	}

	return ordered;
}

CriticalPathComputer::CriticalPathComputer(const Instance & instance_in)
    : l("CritPath"), instance(instance_in)
{}

std::vector<unsigned int>
CriticalPathComputer::get_forward()
{
	using vertex = LagGraph::vertex;
	const LagGraph & graph = this->instance.get_laggraph();

	std::vector<vertex> topological_order = TopologicalSort(graph).get();
	std::vector<unsigned int> earliest_start(graph.vertex_count());

	for (auto v : topological_order) {
		earliest_start[v] = this->instance.get_job(v).get_release();
	}

	for (auto v : topological_order) {
		const auto ec = graph.neighbors(v);
		for (auto edge = ec.cbegin(); edge != ec.cend(); ++edge) {
			unsigned int new_start;
			if (edge->lag < -1 * (int)earliest_start[v]) {
				new_start = 0;
			} else {
				new_start = (unsigned int)((int)earliest_start[v] + edge->lag);
			}

			earliest_start[edge->t] = std::max(earliest_start[edge->t], new_start);
		}
	}

	return earliest_start;
}

std::vector<unsigned int>
CriticalPathComputer::get_reverse()
{
	using vertex = LagGraph::vertex;
	const LagGraph & graph = this->instance.get_laggraph();

	std::vector<vertex> topological_order = TopologicalSort(graph).get();
	std::reverse(topological_order.begin(), topological_order.end());

	std::vector<unsigned int> latest_finish(graph.vertex_count());

	for (auto v : topological_order) {
		latest_finish[v] = this->instance.get_job(v).get_deadline();
	}

	for (auto v : topological_order) {
		for (auto edge : graph.reverse_neighbors(v)) {
			assert(edge.s == v);
			auto t = edge.t;
			const Job & t_job = this->instance.get_job(t);
			const Job & s_job = this->instance.get_job(v);

			int new_finish = (int)latest_finish[v] - (int)s_job.get_duration() +
			                 (int)t_job.get_duration() - (int)edge.lag;

			if (new_finish < 0) {
				BOOST_LOG(l.e()) << "Negative latest finish time detected.";
				BOOST_LOG(l.d()) << "LF-Times computed so far:";
				for (auto w : topological_order) {
					BOOST_LOG(l.d()) << "-> " << w << ": " << latest_finish[w];
					if (w == v) {
						BOOST_LOG(l.d()) << "--> Results in " << new_finish << " for " << t;
						break;
					}
				}

				throw InconsistentDataError(this->instance, -1,
				                            FAULT_CRITICAL_PATH_INFEASIBLE,
				                            "Negative latest finish time");
			}

			if ((unsigned int)new_finish < latest_finish[t]) {
				latest_finish[t] = (unsigned int)new_finish;
			}
		}
	}

	return latest_finish;
}

APLPComputer::APLPComputer(const Instance & instance_in) : instance(instance_in)
{}

std::vector<std::vector<int>>
APLPComputer::get()
{
	this->result.clear();
	this->result.resize(this->instance.job_count(),
	                    std::vector<int>(this->instance.job_count(), -1));

	this->topological_order =
	    TopologicalSort(this->instance.get_laggraph()).get();

	for (unsigned int jid = 0; jid < this->instance.job_count(); ++jid) {
		this->compute_SSLP(jid);
	}

	// TODO this is unsafe! But fast…
	return std::move(this->result);
}

// TODO this can be sped up.
// TODO make this fail if there are negative edges / cycles
void
APLPComputer::compute_SSLP(unsigned int start_job)
{
	const LagGraph & graph = this->instance.get_laggraph();

	this->result[start_job][start_job] = 0;

	auto it = this->topological_order.begin();

	// skip forward to job A
	while (*it != start_job) {
		it++;
	}

	// relax edges
	while (it != topological_order.end()) {
		const unsigned int v = *it;
		it++;

		if (this->result[start_job][v] == -1) { // no path here
			continue;
		}

		for (const auto & edge : graph.neighbors(v)) {
			int relaxed_dist = this->result[start_job][v] + edge.lag;
			assert(relaxed_dist >= 0); // negative paths break things for now

			this->result[start_job][edge.t] =
			    std::max(this->result[start_job][edge.t], relaxed_dist);
		}
	}
}
