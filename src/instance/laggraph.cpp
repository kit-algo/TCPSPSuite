#include "laggraph.hpp"
#include <algorithm>  // for max
#include <cassert>    // for assert
#include <memory>     // for allocator_traits<>::value_type, allocator
#include <utility>    // for pair, make_pair
#include "job.hpp"    // for Job

template<class base>
LagGraph::EdgeContainer::iterator<base>::iterator(const LagGraph::EdgeContainer *c_in, bool end)
  : c(c_in)
{
  if (!end) {
    this->outer = this->c->base_start;

    if (this->outer != this->c->base_end) {
      this->inner_iterator = this->outer->cbegin();

      while ((this->inner_iterator == this->outer->cend()) && (this->outer != this->c->base_end)) {
        this->outer++;
        if (this->outer != this->c->base_end) {
          this->inner_iterator = this->outer->cbegin();
        }
      }

      if (this->outer != this->c->base_end) {
        vertex v = (vertex)std::distance(this->c->index_start, this->outer);
        //assert(v < this->c->g->instance->job_count());
        this->buf = {v, this->inner_iterator->first, this->inner_iterator->second.lag, this->inner_iterator->second.drain_factor, this->inner_iterator->second.max_recharge};
      }
    }
  } else {
    this->outer = this->c->base_end;
  }
}

template<class base>
typename LagGraph::EdgeContainer::iterator<base>::value_type &
LagGraph::EdgeContainer::iterator<base>::operator*()
{
  return this->buf;
}

template<class base>
typename LagGraph::EdgeContainer::iterator<base>::value_type *
LagGraph::EdgeContainer::iterator<base>::operator->()
{
  return &(this->buf);
}

template<class base>
LagGraph::EdgeContainer::iterator<base>
LagGraph::EdgeContainer::iterator<base>::operator++(int) {
  LagGraph::EdgeContainer::iterator<base> old = *this;

  if (this->outer == this->c->base_end) {
    return old;
  }

  // TODO this is not nice. Do we really have to do this twice?
  while ((this->inner_iterator == this->outer->cend()) && (this->outer != this->c->base_end)) {
    this->outer++;
    if (this->outer != this->c->base_end) {
      this->inner_iterator = this->outer->cbegin();
    }
  }

  if (this->outer != this->c->base_end) {
    this->inner_iterator++;
  }

  while ((this->inner_iterator == this->outer->cend()) && (this->outer != this->c->base_end)) {
    this->outer++;
    if (this->outer != this->c->base_end) {
      this->inner_iterator = this->outer->cbegin();
    }
  }

  if (this->outer != this->c->base_end) {
    vertex v = (vertex)std::distance(this->c->index_start, this->outer);
    assert(v < this->c->g->adj.size());
    this->buf = {v, this->inner_iterator->first, this->inner_iterator->second.lag, this->inner_iterator->second.drain_factor, this->inner_iterator->second.max_recharge};
  }

  return old;
}

template<class base>
LagGraph::EdgeContainer::iterator<base>
LagGraph::EdgeContainer::iterator<base>::operator++() {
  this->operator++(1);

  return *this;
}

template<class base>
bool
LagGraph::EdgeContainer::iterator<base>::operator==(const iterator &other) const
{
  if (other.c != this->c) {
    return false;
  }

  // TODO compare base ranges?

  if ((other.outer == this->c->base_end) && (this->outer == this->c->base_end)) {
    // If both are pointing past the end, they are equal independently of inner iterator
    return true;
  }

  return ((other.inner_iterator == this->inner_iterator) && (other.outer == this->outer));
}

template<class base>
bool
LagGraph::EdgeContainer::iterator<base>::operator!=(const iterator &other) const
{
  return !((*this) == other);
}

// instantiate with necessary bases
template class LagGraph::EdgeContainer::iterator<LagGraph::full_edge>;
template class LagGraph::EdgeContainer::iterator<const LagGraph::full_edge>;

/*
 * Implementation of the Edge Container
 */

LagGraph::EdgeContainer::EdgeContainer(const LagGraph *g_in, bool reverse)
  : g(g_in)
{
  if (!reverse) {
    this->base_start = this->g->adj.cbegin();
    this->base_end = this->g->adj.cend();
    this->index_start = this->g->adj.cbegin();
  } else {
    this->base_start = this->g->reverse_adj.cbegin();
    this->base_end = this->g->reverse_adj.cend();
    this->index_start = this->g->reverse_adj.cbegin();
  }
}

LagGraph::EdgeContainer::EdgeContainer(const LagGraph *g_in, unsigned int vertex, bool reverse)
  : g(g_in)
{
  if (!reverse) {
    this->base_start = this->g->adj.cbegin() + vertex;
    this->base_end = this->g->adj.cbegin() + vertex + 1;
    this->index_start = this->g->adj.cbegin();
  } else {
    this->base_start = this->g->reverse_adj.cbegin() + vertex;
    this->base_end = this->g->reverse_adj.cbegin() + vertex + 1;
    this->index_start = this->g->reverse_adj.cbegin();
  }
}

LagGraph::EdgeContainer::EdgeContainer(const LagGraph *g_in, std::vector< std::map< vertex, edge >>::const_iterator start_in, std::vector< std::map< vertex, edge >>::const_iterator end_in, std::vector< std::map< vertex, edge >>::const_iterator index_start_in)
  : g(g_in), base_start(start_in), base_end(end_in), index_start(index_start_in)
{}

LagGraph::EdgeContainer::iterator<LagGraph::full_edge>
LagGraph::EdgeContainer::begin() const
{
  return LagGraph::EdgeContainer::iterator<LagGraph::full_edge>(this, false);
}

LagGraph::EdgeContainer::iterator<LagGraph::full_edge>
LagGraph::EdgeContainer::end() const
{
  return LagGraph::EdgeContainer::iterator<LagGraph::full_edge>(this, true);
}

LagGraph::EdgeContainer::iterator<const LagGraph::full_edge>
LagGraph::EdgeContainer::cbegin() const
{
  return LagGraph::EdgeContainer::iterator<const LagGraph::full_edge>(this, false);
}

LagGraph::EdgeContainer::iterator<const LagGraph::full_edge>
LagGraph::EdgeContainer::cend() const
{
  return LagGraph::EdgeContainer::iterator<const LagGraph::full_edge>(this, true);
}

LagGraph::LagGraph()
 : edge_counter(0)
{}

LagGraph
LagGraph::clone() const
{
  LagGraph cloned;

  cloned.edge_counter = this->edge_counter;
  cloned.adj = this->adj;
  cloned.reverse_adj = this->reverse_adj;
  //cloned.wanted_traits = this->wanted_traits;

  return cloned;
}

LagGraph::vertex
LagGraph::add_vertex()
{
  this->adj.push_back(std::map< vertex, edge >());
  this->reverse_adj.push_back(std::map< vertex, edge>());

#ifdef ENABLE_ASSERTIONS
  assert(this->adj.size() == this->reverse_adj.size());
  assert(this->adj.size() < std::numeric_limits<vertex>::max());
#endif

  return (vertex)this->adj.size() - 1;
}

void
LagGraph::delete_edge(const Job & s, const Job & t)
{
  this->adj[s.get_jid()].erase(t.get_jid());
  this->reverse_adj[t.get_jid()].erase(s.get_jid());
}

void
LagGraph::add_edge(vertex s, vertex t, edge e)
{
  // TODO check this!
  /*
  if (this->wanted_traits.has_flag(Traits::LAGS_ONLY_POSITIVE)) {
    if (e.lag < 0) {
      throw TraitViolatedError("All lags must be positive.\n");
    }
  }
  */
  // TODO this needs to be checked outside!
  /*
  if (this->instance->get_traits().has_flag(Traits::LAGS_ONLY_SUCCESSORS)) {
    int job_length = this->instance->get_job(s).get_duration();

    if (e.lag <= job_length) {
      throw TraitViolatedError("All lags must equal their preceding jobs' length.\n");
    }
  }
  */

  // TODO this is slow
  if (this->adj[s].find(t) == this->adj[s].end()) {
    this->adj[s].insert(std::make_pair(t, e));
    this->reverse_adj[t].insert(std::make_pair(s, e));
    this->edge_counter++;
  }
}

size_t
LagGraph::edge_count() const
{
  return this->edge_counter;
}

LagGraph::edge *
LagGraph::get_edge(vertex s, vertex t)
{
  if (this->adj[s].find(t) != this->adj[s].end()) {
    return &(this->adj[s].find(t)->second);
  } else {
    return nullptr;
  }
}

const LagGraph::edge *
LagGraph::get_edge(vertex s, vertex t) const
{
  if (this->adj[s].find(t) != this->adj[s].end()) {
    return (const LagGraph::edge *) &(this->adj[s].find(t)->second);
  } else {
    return nullptr;
  }
}


void
LagGraph::add_edge(const Job & s, const Job & t, edge e)
{
#ifdef ENABLE_CONSISTENCY_CHECKS
  unsigned int vertex_required = std::max(s.get_jid() , t.get_jid());
  assert(this->adj.size() > vertex_required);
  assert(this->reverse_adj.size() > vertex_required);
#endif

  // Both jobs *have* to be added to the instance!
  //assert(this->instance->get_job(s.get_jid()) == s);
  //assert(this->instance->get_job(t.get_jid()) == t);

  this->add_edge(s.get_jid(), t.get_jid(), e);
}

LagGraph::edge *
LagGraph::get_edge(const Job & s, const Job & t)
{
  if ((s.get_jid() >= this->adj.size()) || (t.get_jid() >= this->adj.size())) {
    return nullptr;
  }

  return this->get_edge(s.get_jid(), t.get_jid());
}

const LagGraph::edge *
LagGraph::get_edge(const Job & s, const Job & t) const
{
  if ((s.get_jid() >= this->adj.size()) || (t.get_jid() >= this->adj.size())) {
    return nullptr;
  }

  return (const LagGraph::edge *) this->get_edge(s.get_jid(), t.get_jid());
}

LagGraph::EdgeContainer
LagGraph::edges()
{
  return LagGraph::EdgeContainer(this);
}

const LagGraph::EdgeContainer
LagGraph::edges() const
{
  return LagGraph::EdgeContainer(this);
}

#ifdef ENABLE_CONSISTENCY_CHECKS
void
LagGraph::check_consistency()
{
  this->check_edge_iterator_consistency();
}
#endif

void
LagGraph::check_edge_iterator_consistency()
{
  for (auto edge_it : this->edges()) {
    if (this->get_edge(edge_it.s, edge_it.t) == nullptr) {
      assert(false);
    }
  }
}

LagGraph::EdgeContainer
LagGraph::reverse_edges()
{
  return LagGraph::EdgeContainer(this, true);
}

const LagGraph::EdgeContainer
LagGraph::reverse_edges() const
{
  return LagGraph::EdgeContainer(this, true);
}

const LagGraph::EdgeContainer
LagGraph::neighbors(vertex v) const
{
  return LagGraph::EdgeContainer(this, v, false);
}

size_t
LagGraph::vertex_count() const
{
  return this->adj.size();
}

size_t
LagGraph::neighbor_count(vertex v) const
{
  return this->adj[v].size();
}

size_t
LagGraph::reverse_neighbor_count(vertex v) const
{
  return this->reverse_adj[v].size();
}

const LagGraph::EdgeContainer
LagGraph::reverse_neighbors(vertex v) const
{
  return LagGraph::EdgeContainer(this, v, true);
}
