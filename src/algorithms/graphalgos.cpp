#include "graphalgos.hpp"

#include "../manager/errors.hpp"   // for InconsistentDataError
#include "../util/fault_codes.hpp" // for FAULT_CRITICAL_PATH_INFEASIBLE
#include "instance/instance.hpp"   // for Instance
#include "instance/job.hpp"        // for Job
#include "instance/laggraph.hpp"   // for LagGraph, LagGraph::(anonymous)

#include <algorithm>           // for move, max, reverse
#include <assert.h>            // for assert
#include <bits/std_function.h> // for function
#include <boost/range/adaptor/reversed.hpp>
#include <ext/alloc_traits.h> // for __alloc_traits<>::value_type
#include <functional>         // for function
#include <string>             // for string
#include <unordered_set>

NecessaryOrderComputer::NecessaryOrderComputer(const Instance & instance_in)
    : instance(instance_in)
{
	TopologicalSort ts(instance.get_laggraph());
	this->topo_order = ts.get();

	CriticalPathComputer cp(this->instance);
	this->earliest_starts = cp.get_forward();
	this->latest_finishs = cp.get_reverse();

	// We also need to obey latest finish -> earliest start relationship
	std::stable_sort(this->topo_order.begin(), this->topo_order.end(),
	                 [&](unsigned int lhs, unsigned int rhs) {
		                 return latest_finishs[lhs] < earliest_starts[rhs];
	                 });
}

std::vector<size_t>
NecessaryOrderComputer::get_predecessor_count() const
{
	std::vector<size_t> result(this->instance.job_count(), 0);
	std::vector<std::vector<LagGraph::vertex>> predecessors(
	    this->instance.job_count());
	std::vector<size_t> next_end_index(this->instance.job_count(), 0);
	std::vector<std::pair<unsigned int, LagGraph::vertex>> end_events;

	for (unsigned int v = 0; v < this->instance.job_count(); ++v) {
		end_events.push_back({this->latest_finishs[v], v});
	}
	std::sort(end_events.begin(), end_events.end(), [](auto & lhs, auto & rhs) {
		if (lhs.first != rhs.first) {
			return lhs.first < rhs.first;
		}
		return lhs.second < rhs.second;
	});

	// TODO FIXME this only works for lag >= duration!

	for (unsigned int jid : this->topo_order) {
		// Factor in definitely finished jobs
		std::vector<LagGraph::vertex> additional;
		additional.push_back(jid); // We subtract that below
		while (end_events[next_end_index[jid]].first <= earliest_starts[jid]) {
			additional.push_back(end_events[next_end_index[jid]].second);
			next_end_index[jid]++;
		}

		std::sort(additional.begin(), additional.end());
		
		std::vector<LagGraph::vertex> new_predecessors;
		new_predecessors.reserve(predecessors[jid].size() + additional.size());
		std::set_union(predecessors[jid].begin(), predecessors[jid].end(),
		               additional.begin(), additional.end(),
		               std::back_inserter(new_predecessors));

		result[jid] = new_predecessors.size() - 1; // subtract ourselves
		for (const auto & neigh : this->instance.get_laggraph().neighbors(jid)) {
			std::vector<LagGraph::vertex> push_to_neighbor;
			push_to_neighbor.reserve(predecessors[neigh.t].size() +
			                         new_predecessors.size());
			std::set_union(new_predecessors.begin(), new_predecessors.end(),
			               predecessors[neigh.t].begin(), predecessors[neigh.t].end(),
			               std::back_inserter(push_to_neighbor));
			predecessors[neigh.t] = std::move(push_to_neighbor);
			next_end_index[neigh.t] =
			    std::max(next_end_index[neigh.t], next_end_index[jid]);
		}

		// We never need the predecessors of jid again. Save some memory.
		predecessors[jid].clear();
		predecessors[jid].shrink_to_fit();
	}

	return result;
}

std::vector<size_t>
NecessaryOrderComputer::get_successor_count() const
{
	std::vector<size_t> result(this->instance.job_count(), 0);
	std::vector<std::vector<LagGraph::vertex>> successors(
	    this->instance.job_count());
	std::vector<size_t> next_start_index(this->instance.job_count(), 0);
	std::vector<std::pair<unsigned int, LagGraph::vertex>> start_events;

	for (unsigned int v = 0; v < this->instance.job_count(); ++v) {
		start_events.push_back({earliest_starts[v], v});
	}
	std::sort(start_events.begin(), start_events.end(),
	          [](auto & lhs, auto & rhs) {
		          if (lhs.first != rhs.first) {
			          return lhs.first > rhs.first;
		          }
		          return lhs.second < rhs.second;
	          });

	// TODO FIXME this only works for lag >= duration!

	for (unsigned int jid : boost::adaptors::reverse(this->topo_order)) {
		// Factor in definitely started jobs
		std::vector<LagGraph::vertex> additional;
		additional.push_back(jid); // We subtract that below
		while (start_events[next_start_index[jid]].first >=
		       this->latest_finishs[jid]) {
			additional.push_back(start_events[next_start_index[jid]].second);
			next_start_index[jid]++;
		}

		std::sort(additional.begin(), additional.end());
		
		std::vector<LagGraph::vertex> new_successors;
		new_successors.reserve(successors[jid].size() + additional.size());
		std::set_union(successors[jid].begin(), successors[jid].end(),
		               additional.begin(), additional.end(),
		               std::back_inserter(new_successors));

		result[jid] = new_successors.size() - 1; // subtract ourselves
		for (const auto & rev_neigh :
		     this->instance.get_laggraph().reverse_neighbors(jid)) {

			assert(rev_neigh.s == jid); // TODO DEBUG REMOVE
			std::vector<LagGraph::vertex> push_to_neighbor;
			push_to_neighbor.reserve(successors[rev_neigh.t].size() +
			                         new_successors.size());

			std::set_union(new_successors.begin(), new_successors.end(),
			               successors[rev_neigh.t].begin(),
			               successors[rev_neigh.t].end(),
			               std::back_inserter(push_to_neighbor));
			successors[rev_neigh.t] = std::move(push_to_neighbor);
			next_start_index[rev_neigh.t] =
			    std::max(next_start_index[rev_neigh.t], next_start_index[jid]);
		}

		// We never need the predecessors of jid again. Save some memory.
		successors[jid].clear();
		successors[jid].shrink_to_fit();
	}

	return result;
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
    : instance(instance_in)
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
