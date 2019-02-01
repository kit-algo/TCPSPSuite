#include "traits.hpp"
#include <bits/std_function.h>           // for function
#include <stdlib.h>                      // for size_t, exit
#include <algorithm>                     // for reverse
#include <iostream>                      // for cout
#include <limits>                        // for numeric_limits
#include "../algorithms/graphalgos.hpp"  // for CriticalPathComputer, Topolo...
#include "../datastructures/maybe.hpp"   // for Maybe
#include "generated_config.hpp"          // for DOUBLE_DELTA
#include "instance.hpp"                  // for Instance
#include "job.hpp"                       // for Job
#include "laggraph.hpp"                  // for LagGraph, LagGraph::vertex
#include "resource.hpp"                  // for Resource, polynomial, Availa...
#include "transform.hpp"                 // for Transformer

const unsigned long Traits::NO_LAGS                    = 1 << 0;
const unsigned long Traits::LAGS_ONLY_SUCCESSORS       = 1 << 1;
const unsigned long Traits::LAGS_ONLY_GREATER_DURATION = 1 << 2;
const unsigned long Traits::LAGS_ONLY_POSITIVE         = 1 << 3;
const unsigned long Traits::LAGS_DAG                   = 1 << 4;

const unsigned long Traits::COMMON_RELEASE             = 1 << 5;
const unsigned long Traits::COMMON_DEADLINE            = 1 << 6;
const unsigned long Traits::COMMON_DURATION            = 1 << 7;

const unsigned long Traits::CONSISTENT_WINDOWS         = 1 << 8;


const unsigned long Traits::DUMMY_START_END            = 1 << 9;

const unsigned long Traits::NO_DRAIN                   = 1 << 10;

const unsigned long Traits::NO_WINDOW_EXTENSION        = 1 << 11;
const unsigned long Traits::WINDOW_EXTENSION_JOBS_UNLIMITED  = 1 << 12;

const unsigned long Traits::FLAT_AVAILABILITY         = 1 << 13;
const unsigned long Traits::ZERO_AVAILABILITY         = 1 << 14;

const char* const Traits::FLAG_NAMES[] = {
  "NO_LAGS",
  "LAGS_ONLY_SUCCESSORS",
  "LAGS_ONLY_GREATER_DURATION",
  "LAGS_ONLY_POSITIVE",
  "LAGS_DAG",

  "COMMON_RELEASE",
  "COMMON_DEADLINE",
  "COMMON_DURATION",

  "CONSISTENT_WINDOWS",

  "DUMMY_START_END",

  "NO_DRAIN",

  "NO_WINDOW_EXTENSION",
  "WINDOW_EXTENSION_JOBS_UNLIMITED",

  "FLAT_AVAILABILITY",
  "ZERO_AVAILABILITY"
};

// TraitsRouter
const unsigned short TraitsRouter::WANT_MAYBE          = 0;
const unsigned short TraitsRouter::WANT_YES            = 1;
const unsigned short TraitsRouter::WANT_NO             = 2;



// TODO do we need these?
const std::vector<std::pair<unsigned long, unsigned long>> Traits::implications =
std::vector<std::pair<unsigned long, unsigned long>>({
  {NO_LAGS, LAGS_ONLY_SUCCESSORS},
  {LAGS_ONLY_SUCCESSORS, LAGS_ONLY_GREATER_DURATION},
  {LAGS_ONLY_GREATER_DURATION, LAGS_ONLY_POSITIVE},

  {NO_LAGS, LAGS_DAG},

  {NO_LAGS, NO_DRAIN},

  {ZERO_AVAILABILITY, FLAT_AVAILABILITY}
});

TraitViolatedError::TraitViolatedError(const std::string &what)
  : std::runtime_error(what)
{}

TraitViolatedError::TraitViolatedError(const char *what)
  : std::runtime_error(what)
{}

TraitUnfulfilledError::TraitUnfulfilledError(const std::string &what)
  : std::runtime_error(what)
{}

TraitUnfulfilledError::TraitUnfulfilledError(const char *what)
  : std::runtime_error(what)
{}


Traits::Traits(unsigned long flags_in, unsigned int max_resources_in, std::set<double> allowed_overshoot_exponents_in, std::set<double> allowed_investment_exponents_in)
  : flags(flags_in), max_resources(max_resources_in), allowed_overshoot_exponents(allowed_overshoot_exponents_in),
    allowed_investment_exponents(allowed_investment_exponents_in)
{}

Traits::Traits()
  : flags(0), max_resources(std::numeric_limits<unsigned int>::max())
{}

Traits
Traits::clone() const
{
  Traits cloned(this->flags, this->max_resources, this->allowed_overshoot_exponents, this->allowed_investment_exponents);

  return cloned;
}

bool
Traits::has_flag(unsigned long flag) const
{
  return this->flags & flag;
}

void
Traits::add_flag(unsigned long flag)
{
  this->flags |= (flag);
}

void
Traits::remove_flag(unsigned long flag)
{
  this->flags &= !(flag);
}


bool
Traits::fulfills(const Traits &requirements) const
{
  if (this->max_resources > requirements.max_resources) {
    return false;
  }

  if (requirements.allowed_investment_exponents.size() > 0) {
    if (this->allowed_investment_exponents.size() == 0) {
      return false; // We allow everything, they specify a set
    }

    for (auto exponent : this->allowed_investment_exponents) {
      if (requirements.allowed_investment_exponents.find(exponent) == requirements.allowed_investment_exponents.end()) {
        return false;
      }
    }
  }

  if (requirements.allowed_overshoot_exponents.size() > 0) {
    if (this->allowed_overshoot_exponents.size() == 0) {
      return false; // We allow everything, they specify a set
    }

    for (auto exponent : this->allowed_overshoot_exponents) {
      if (requirements.allowed_overshoot_exponents.find(exponent) == requirements.allowed_overshoot_exponents.end()) {
        return false;
      }
    }
  }

  return !(requirements.flags & !(this->flags));
}

TraitsBuilder::TraitsBuilder(const Instance &instance_in)
  : flags(0), instance(instance_in)
{}

void
TraitsBuilder::check_window_extension()
{
  if ((this->instance.get_window_extension_job_limit() == 0) ||
          (this->instance.get_window_extension_limit() == 0)) {
    this->flags |= Traits::NO_WINDOW_EXTENSION;
    return;
  }

  if (this->instance.get_window_extension_job_limit() >= this->instance.job_count()) {
    this->flags |= Traits::WINDOW_EXTENSION_JOBS_UNLIMITED;
  }
}

void
TraitsBuilder::check_no_drain()
{
  bool no_drain = true;
  for (auto & edge : this->instance.get_laggraph().edges()) {
    if (edge.max_recharge > 0) {
      no_drain = false;
    }
  }

  if (no_drain) {
    this->flags |= Traits::NO_DRAIN;
  }
}

void
TraitsBuilder::check_no_lags()
{
  if (this->instance.get_laggraph().edge_count() == 0) {
    this->flags |= Traits::NO_LAGS;
  }
}

void
TraitsBuilder::check_availabilities()
{
  bool all_zero = true;
  bool all_flat = true;

  for (unsigned int rid = 0 ; rid < this->instance.resource_count() ; ++rid) {
    const Resource & res = this->instance.get_resource(rid);
    double amount = res.get_availability().begin()->second;
    if (amount > 0) {
      all_zero = false;
    }

    for (const auto & point : res.get_availability()) {
      if ((point.second > amount + DOUBLE_DELTA) || (point.second < amount - DOUBLE_DELTA)) {
        all_flat = false;
      }
    }
  }

  if (all_zero) {
    this->flags |= Traits::ZERO_AVAILABILITY;
  }

  if (all_flat) {
    this->flags |= Traits::FLAT_AVAILABILITY;
  }
}

void
TraitsBuilder::check_lag_durations()
{
  bool only_successors = true;
  bool only_positive = true;
  bool only_greater_duration = true;

  for (auto & edge : this->instance.get_laggraph().edges()) {
    auto s_id = edge.s;
    int lag = edge.lag;

    if (lag < 0) {
      only_positive = false;
    }

    const Job & s = this->instance.get_job(s_id);

    if ((lag < 0) || ((unsigned int)lag < s.get_duration())) {
      only_greater_duration = false;
    }

    if ((lag < 0) || ((unsigned int)lag != s.get_duration())) {
      only_successors = false;
    }
  }

  if (only_successors) {
    this->flags |= Traits::LAGS_ONLY_SUCCESSORS;
  }

  if (only_positive) {
    this->flags |= Traits::LAGS_ONLY_POSITIVE;
  }

  if (only_greater_duration) {
    this->flags |= Traits::LAGS_ONLY_GREATER_DURATION;
  }
}

void
TraitsBuilder::check_consistent_windows()
{
  CriticalPathComputer cpc(this->instance);

  std::vector<unsigned int> earliest_starts = cpc.get_forward();
  std::vector<unsigned int> latest_finish = cpc.get_reverse();

  for (const auto & job : this->instance.get_jobs()) {
    if (job.get_release() < earliest_starts[job.get_jid()]) {
      return;
    }

    if (job.get_deadline() > latest_finish[job.get_jid()]) {
      return;
    }
  }

  this->flags |= (Traits::CONSISTENT_WINDOWS);
}

void
TraitsBuilder::check_dummy_start_end()
{
  // There are three conditions: The graph is connected, has one source, and one sink!
  using vertex = LagGraph::vertex;

  vertex source;
  bool source_found = false;
  bool sink_found = false;

  for (const auto & job : this->instance.get_jobs()) {
    if (this->instance.get_laggraph().reverse_neighbor_count(job.get_jid()) == 0) {
      // no incoming, i.e., a source
      if (source_found) {
        // multiple sources: no dummies
        return;
      }

      source_found = true;
      source = job.get_jid();
    }

    if (this->instance.get_laggraph().neighbor_count(job.get_jid()) == 0) {
      // no outgoing, i.e., sink
      if (sink_found) {
        // multiple sinks
        return;
      }
      sink_found = true;
    }
  }

  std::vector<bool> visited(this->instance.job_count(), false);
  unsigned int visit_count = 0;

  std::function<bool(LagGraph::vertex,LagGraph::vertex)> visit = [&](LagGraph::vertex v, LagGraph::vertex from) {
    (void)from;

    if (!visited[v]) {
      visit_count++;
    }
    visited[v] = true;

    return true;
  };
  std::function<void(LagGraph::vertex,LagGraph::vertex,int)> traverse = [&](LagGraph::vertex from, LagGraph::vertex to, int lag) {
    (void)from;
    (void)to;
    (void)lag;
  };
  std::function<void(LagGraph::vertex)> backtrack = [&](unsigned int v) {
    (void)v;
  };

  if (!source_found) {
    return;
  }

  DFS<decltype(visit), decltype(backtrack), decltype(traverse)>(this->instance.get_laggraph(), source, visit, backtrack, traverse, false);

  if (visit_count == this->instance.job_count()) {
    // connected!
    this->flags |= Traits::DUMMY_START_END;
  }

}

void
TraitsBuilder::check_lag_dag()
{
  auto topo_order = TopologicalSort(this->instance.get_laggraph()).get();
  std::vector<size_t> topo_index(this->instance.job_count());

  size_t i = 0;
  for (const auto & v : topo_order) {
    topo_index[v] = i++;
  }

  bool is_dag = true;

  for (const auto & edge : this->instance.get_laggraph().edges()) {
    if (topo_index[edge.s] > topo_index[edge.t]) {
      is_dag = false;
    }
  }

  if (is_dag) {
    this->flags |= Traits::LAGS_DAG;
  }
}

void
TraitsBuilder::check_deadline_release()
{
  bool has_common_deadline = true;
  bool has_common_duration = true;
  bool has_common_release = true;

  if (this->instance.job_count() > 0) {
    unsigned int common_deadline = this->instance.get_job(0).get_deadline();
    unsigned int common_release = this->instance.get_job(0).get_release();
    unsigned int common_duration = this->instance.get_job(0).get_duration();

    for (unsigned int job_id = 1 ; job_id  < this->instance.job_count() ; ++job_id) {
      if (this->instance.get_job(job_id).get_deadline() != common_deadline) {
        has_common_deadline = false;
      }
      if (this->instance.get_job(job_id).get_duration() != common_duration) {
        has_common_duration = false;
      }
      if (this->instance.get_job(job_id).get_release() != common_release) {
        has_common_release = false;
      }

      if (!has_common_release && !has_common_duration && !has_common_deadline) {
        break;
      }
    }
  }

  if (has_common_release) {
    this->flags |= Traits::COMMON_RELEASE;
  }
  if (has_common_duration) {
    this->flags |= Traits::COMMON_DURATION;
  }
  if (has_common_deadline) {
    this->flags |= Traits::COMMON_DEADLINE;
  }
}

void
TraitsBuilder::run()
{
  this->check_no_lags();
  this->check_lag_durations();
  this->check_lag_dag();

  this->check_no_drain();

  this->check_deadline_release();
  this->check_consistent_windows();

  this->check_dummy_start_end();

  this->check_window_extension();
  
  this->check_availabilities();
}

Traits
TraitsBuilder::get_traits()
{
  std::set<double> overshoot_exponents;
  std::set<double> investment_exponents;

  for (unsigned int r_id = 0 ; r_id < this->instance.resource_count() ; ++r_id) {
    polynomial overshoot_costs = this->instance.get_resource(r_id).get_overshoot_costs();
    for (std::pair<double, double> & term : overshoot_costs) {
      overshoot_exponents.insert(std::get<1>(term));
    }

    polynomial investment_costs = this->instance.get_resource(r_id).get_investment_costs();
    for (std::pair<double, double> & term : investment_costs) {
      investment_exponents.insert(std::get<1>(term));
    }
  }

  return Traits(this->flags, this->instance.resource_count(), overshoot_exponents, investment_exponents);
}

TraitsRouter::TraitsRouter(const std::set<Transformer *> & transformers_in)
  : transformers(transformers_in)
{
  this->build_in_edges();
}

void
TraitsRouter::build_in_edges()
{
  for (Transformer * trans : this->transformers) {
    this->in_edges.push_back(std::make_pair(trans->get_requirements(), trans));
  }
}

bool
TraitsRouter::match(const trait_profile & traits_p, const transform_profile & trans_p) const
{
  for (unsigned int i = 0 ; i < traits_p.size() ; ++i) {
    if ((trans_p[i] == WANT_YES) && (traits_p[i] == false)) {
      return false;
    }
  }

  return true;
}

bool
TraitsRouter::fulfills(const trait_profile & profile, const trait_profile & pattern) const
{
  for (unsigned int i = 0 ; i < profile.size() ; ++i) {
    if ((pattern[i]) && (!profile[i])) {
      return false;
    }
  }

  return true;
}

std::vector<Transformer *>
TraitsRouter::find_matching(const trait_profile & profile) const
{
  std::vector<Transformer *> result;

  for (const auto & in_edge : this->in_edges) {
    if (this->match(profile, std::get<0>(in_edge))) {
      result.push_back(std::get<1>(in_edge));
    }
  }

  return result;
}

TraitsRouter::trait_profile
TraitsRouter::transform_flags(const trait_profile & in_profile, const transform_profile & transform) const {
  trait_profile result;

  for (unsigned int i = 0 ; i < in_profile.size() ; ++i) {
    if (transform[i] == WANT_MAYBE) {
      result[i] = in_profile[i];
    } else {
      result[i] = (transform[i] == WANT_YES);
    }
  }

  return result;
}

TraitsRouter::trait_profile
TraitsRouter::traits_to_profile(const Traits & traits) const
{
  trait_profile result;

  for (auto flag : ALL_TRAIT_FLAGS) {
    result[flag_to_index(flag)] = traits.has_flag(flag);
  }

  return result;
}

Maybe<std::vector<Transformer *>>
TraitsRouter::get_path(const Traits & from, const Traits & to)
{
  trait_profile from_profile = this->traits_to_profile(from);
  trait_profile to_profile = this->traits_to_profile(to);

  if (this->fulfills(from_profile, to_profile)) {
    return std::vector<Transformer *>();
  }

  this->do_bfs(from, to);

  if (!this->found) {
    return Maybe<std::vector<Transformer *>>();
  } else {
    std::vector<Transformer *> result;
    trait_profile cur = this->final_profile;

    ///std::cout << "Looking for profile: ";
    for (auto flag : ALL_TRAIT_FLAGS) {
      if (from_profile[flag_to_index(flag)]) {
        std::cout << Traits::FLAG_NAMES[flag_to_index(flag)] << ",";
      }
    }
    //std::cout << "\n";

    int i = 0;
    while (cur != from_profile) {
      /*
      std::cout << "Current profile in tree: ";
      for (auto flag : ALL_TRAIT_FLAGS) {
        if (cur[flag_to_index(flag)]) {
          std::cout << Traits::FLAG_NAMES[flag_to_index(flag)] << ",";
        }
      }
      std::cout << "\n";
      */
      result.push_back(this->tree.at(cur).first);
      cur = this->tree.at(cur).second;

      if (i > 2) {
        exit(-1);
      }
      i++;
    }

    std::reverse(result.begin(), result.end());

    return Maybe<std::vector<Transformer *>>(result);
  }
}

// TODO FIXME check for other conditions (no. of resources, etc)
void
TraitsRouter::do_bfs(const Traits & from , const Traits & to)
{
  trait_profile from_profile = this->traits_to_profile(from);
  trait_profile to_profile = this->traits_to_profile(to);

  this->queue.clear();
  this->tree.clear();

  this->queue.push_back(from_profile);
  this->tree.insert(std::make_pair(from_profile, std::make_pair(nullptr, from_profile)));

  this->found = false;
  while ((! this->found) && (this->queue.size() > 0)) {
    const trait_profile & profile = this->queue.front();

    std::cout << "Current profile: ";
    for (auto flag : ALL_TRAIT_FLAGS) {
      if (profile[flag_to_index(flag)]) {
        std::cout << Traits::FLAG_NAMES[flag_to_index(flag)] << ",";
      }
    }
    std::cout << "\n";

    for (auto transformer : this->find_matching(profile)) {
      trait_profile out = this->transform_flags(profile, transformer->get_transformation());

      if (this->tree.find(out) == this->tree.end()) {
        std::cout << "INSERTING: \n";
        for (auto flag : ALL_TRAIT_FLAGS) {
          if (out[flag_to_index(flag)]) {
            std::cout << Traits::FLAG_NAMES[flag_to_index(flag)] << ",";
          }
        }

        std::cout << "\n   from \n";

        for (auto flag : ALL_TRAIT_FLAGS) {
          if (profile[flag_to_index(flag)]) {
            std::cout << Traits::FLAG_NAMES[flag_to_index(flag)] << ",";
          }
        }
        std::cout << "\n";

        this->tree.insert(std::make_pair(out, std::make_pair(transformer, profile)));
        queue.push_back(out);

        if (this->fulfills(out, to_profile)) {
          this->found = true;
          this->final_profile = out;
          break;
        }
      }
    }

    this->queue.pop_front();
  }
}
