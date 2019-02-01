#ifndef GRAPHALGOS_H
#define GRAPHALGOS_H

#include <vector>                    // for vector
#include "../instance/laggraph.hpp"  // for LagGraph, LagGraph::vertex
class Instance;

template <typename visit_func, typename backtrack_func, typename traverse_func>
class DFS {
public:
  using vertex = LagGraph::vertex;

  DFS(const LagGraph & graph, const LagGraph::vertex start, visit_func visit, backtrack_func backtrack, traverse_func traverse, bool reverse = false);
private:
  visit_func visit;
  backtrack_func backtrack;
  traverse_func traverse;
  const LagGraph &graph;

  std::vector<bool> visited;

  void dfs_rec(vertex v, vertex from, bool reverse) ;
};


class TopologicalSort
{
public:
  TopologicalSort(const LagGraph & graph);
  std::vector<LagGraph::vertex> get();

private:
  const LagGraph & graph;
};

class CriticalPathComputer
{
public:
  CriticalPathComputer(const Instance & instance);
  std::vector<unsigned int> get_forward();
  std::vector<unsigned int> get_reverse();

private:
  const Instance & instance;
};

class APLPComputer
{
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
