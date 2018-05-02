#include "graphalgos.hpp"
#include <assert.h>                             // for assert
#include <ext/alloc_traits.h>                   // for __alloc_traits<>::val...
#include <algorithm>                            // for max, move, reverse
#include <functional>                           // for function
#include "../util/fault_codes.hpp"              // for FAULT_CRITICAL_PATH_I...
#include "../manager/errors.hpp"

TopologicalSort::TopologicalSort(const LagGraph & graph_in)
  : graph(graph_in)
{}

std::vector<LagGraph::vertex>
TopologicalSort::get()
{
  using vertex = LagGraph::vertex;

  std::vector<unsigned int> indices(this->graph.vertex_count(), 0);

  unsigned int index = (unsigned int)(this->graph.vertex_count() - 1);

  std::function<bool(vertex,vertex)> visit = [&](vertex v, vertex from) {
    (void)from;
    (void)v;
    return true;
  };
  std::function<void(vertex,vertex,int)> traverse = [&](vertex from, vertex to, int lag) {
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

  for (vertex v = 0 ; v < this->graph.vertex_count() ; ++v) {
    if (this->graph.reverse_neighbor_count(v) > 0) {
      continue; // Only select vertices without incoming edges as roots
    }
    assert(indices[v] == 0);
    DFS<decltype(visit), decltype(backtrack), decltype(traverse)>(this->graph, v, visit, backtrack, traverse);

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
  for (vertex v = 0 ; v < this->graph.vertex_count() ; ++v) {
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
    for (auto edge = ec.cbegin() ; edge != ec.cend() ; ++edge) {
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
    for (const auto & edge : graph.reverse_neighbors(v)) {
      assert(edge.s == v);
      auto t = edge.t;
      auto t_job = this->instance.get_job(t);
      auto s_job = this->instance.get_job(v);

      int new_finish = (int)latest_finish[t] - (int)t_job.get_duration() + (int)s_job.get_duration() - (int)edge.lag;

      if (new_finish < 0) {
        throw InconsistentDataError(this->instance, -1, FAULT_CRITICAL_PATH_INFEASIBLE, "Negative latest finish time");
      }

      if ((unsigned int)new_finish < latest_finish[v]) {
        latest_finish[v] = (unsigned int)new_finish;
      }
    }
  }

  return latest_finish;
}

APLPComputer::APLPComputer(const Instance & instance_in)
  : instance(instance_in)
{}

std::vector<std::vector<int>>
APLPComputer::get()
{
  this->result.clear();
  this->result.resize(this->instance.job_count(), std::vector<int>(this->instance.job_count(), -1));

  this->topological_order = TopologicalSort(this->instance.get_laggraph()).get();

  for (unsigned int jid = 0 ; jid < this->instance.job_count() ; ++jid) {
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

      this->result[start_job][edge.t] = std::max(this->result[start_job][edge.t], relaxed_dist);
    }
  }
}
