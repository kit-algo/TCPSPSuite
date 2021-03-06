#ifndef GRAPHALGOS_H
#define GRAPHALGOS_H

#include "../instance/laggraph.hpp" // for LagGraph, LagGraph::vertex
#include "../util/log.hpp"

#include <vector> // for vector
class Instance;

template <typename visit_func, typename backtrack_func, typename traverse_func>
class DFS {
public:
	using vertex = LagGraph::vertex;

	DFS(const LagGraph & graph, const LagGraph::vertex start, visit_func visit,
	    backtrack_func backtrack, traverse_func traverse, bool reverse = false);

private:
	visit_func visit;
	backtrack_func backtrack;
	traverse_func traverse;
	const LagGraph & graph;

	std::vector<bool> visited;

	void dfs_rec(vertex v, vertex from, bool reverse);
};

class TopologicalSort {
public:
	TopologicalSort(const LagGraph & graph);
	std::vector<LagGraph::vertex> get();

private:
	const LagGraph & graph;
};

// TODO this is currently only based on earliest starts / latest finishs.
// We should incorporate dependency path length here!
class NecessaryOrderComputer {
public:
	NecessaryOrderComputer(const Instance & instance);
	// Returns, for each job, the minimum number of jobs that must be finished
	// before the respective job can start.
	const std::vector<size_t> & get_predecessor_count() const noexcept;
	// Returns, for each job, the minimum number of jobs that cannot have started
	// before the respective job finishes
	const std::vector<size_t> & get_successor_count() const noexcept;

private:
	std::vector<size_t> predecessor_count;
	std::vector<size_t> successor_count;

	const Instance & instance;

	void compute();

	std::vector<unsigned int> earliest_starts;
	std::vector<unsigned int> latest_finishs;
};

class CriticalPathComputer {
public:
	CriticalPathComputer(const Instance & instance);
	std::vector<unsigned int> get_forward();
	std::vector<unsigned int> get_reverse();

private:
	Log l;
	const Instance & instance;
};

class APLPComputer {
public:
	APLPComputer(const Instance & instance);
	// -1 as path length means 'no path'
	std::vector<std::vector<int>> get();

private:
	std::vector<std::vector<int>> result;
	std::vector<unsigned int> topological_order;

	void compute_SSLP(unsigned int start_job);

	const Instance & instance;
};

#include "graphalgos_templates.cpp"

#endif
