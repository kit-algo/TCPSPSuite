#ifndef LAGGRAPH_HPP
#define LAGGRAPH_HPP

#include <cstdlib>               // for size_t
#include <iterator>              // for input_iterator_tag
#include <limits>                // for numeric_limits
#include <map>                   // for map, map<>::const_iterator
#include <vector>                // for vector, vector<>::const_iterator
#include "generated_config.hpp"  // for ENABLE_CONSISTENCY_CHECKS
class Job;  // lines 10-10

class LagGraph {
public:
  typedef unsigned int vertex;
  static const vertex no_vertex = std::numeric_limits<vertex>::max();

  typedef struct { int lag; double drain_factor; unsigned int max_recharge; } edge;
  typedef struct { vertex s; vertex t; int lag; double drain_factor; unsigned int max_recharge; } full_edge;


  class EdgeContainer {
  public:
    typedef full_edge          value_type;
    typedef full_edge          *pointer;
    typedef const full_edge    *const_pointer;
    typedef full_edge          &reference;
    typedef const full_edge    &const_reference;
    typedef size_t             size_type;

    template<class base>
    class iterator {
    public:
      typedef std::input_iterator_tag   iterator_category;
      typedef base                      value_type;
      typedef base                      *pointer;
      typedef base                      &reference;
      typedef size_t                    size_type;
      //typedef ptrdiff_t                 difference_type;

      iterator(const EdgeContainer *c, bool end = false);

      value_type &operator*();
      value_type *operator->();

      iterator<base> operator++(int);
      iterator<base> operator++();

      bool operator==(const iterator<base> &other) const;
      bool operator!=(const iterator<base> &other) const;

    private:
      const EdgeContainer *c;
      std::vector< std::map< vertex, edge >>::const_iterator outer;
      std::map<vertex, edge>::const_iterator inner_iterator;

      full_edge buf;
    };

    EdgeContainer(const LagGraph *g, bool reverse = false);
    EdgeContainer(const LagGraph *g, unsigned int vertex, bool reverse = false);
    EdgeContainer(const LagGraph *g,
                  std::vector< std::map< vertex, edge >>::const_iterator start,
                  std::vector< std::map< vertex, edge >>::const_iterator end,
                  std::vector< std::map< vertex, edge >>::const_iterator index_start);

    // TODO why do these two have to be const? I don't get it.
    iterator<full_edge> begin() const;
    iterator<full_edge> end() const;
    iterator<const full_edge> cbegin() const;
    iterator<const full_edge> cend() const;


  private:
    const LagGraph *g;
    std::vector< std::map< vertex, edge >>::const_iterator base_start;
    std::vector< std::map< vertex, edge >>::const_iterator base_end;
    std::vector< std::map< vertex, edge >>::const_iterator index_start;
    };

  LagGraph();

  void set_limitations(unsigned long limitations);

  vertex add_vertex();
  void add_edge(const Job & s, const Job & t, edge lag);
  void delete_edge(const Job &s, const Job &t);

  edge *get_edge(const Job & s, const Job & t);
  const edge *get_edge(const Job & s, const Job & t) const;
  edge *get_edge(vertex s, vertex t);
  const edge *get_edge(vertex s, vertex t) const;

  EdgeContainer edges();
  const EdgeContainer edges() const;
  EdgeContainer reverse_edges();
  const EdgeContainer reverse_edges() const;

  // TODO rewrite this to const Job &!
  const EdgeContainer neighbors(vertex v) const;
  const EdgeContainer reverse_neighbors(vertex v) const;

  size_t neighbor_count(vertex v) const;
  size_t reverse_neighbor_count(vertex v) const;
  size_t edge_count() const;
  size_t vertex_count() const;

/*
  // Public Algorithms
  std::map<unsigned int, int> critical_start_times();
  std::map<unsigned int, int> critical_end_times();
*/

  #ifdef ENABLE_CONSISTENCY_CHECKS
      void check_consistency();
  #else
      inline void check_consistency() {};
  #endif

  // deepcopy
  LagGraph clone() const;

private:
  void add_edge(vertex s, vertex t, edge e);

  size_t edge_counter;

  std::vector< std::map< vertex, edge >> adj;
  std::vector< std::map< vertex, edge >> reverse_adj;

  void check_edge_iterator_consistency();
};

#endif
