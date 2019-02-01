#include "transform.hpp"
#include "../algorithms/graphalgos.hpp" // for CriticalP...
#include "../instance/job.hpp"          // for Job
#include "../instance/traits.hpp"       // for TraitsRouter
#include "../manager/errors.hpp"        // for Inconsist...
#include "../util/fault_codes.hpp"      // for FAULT_CRI...
#include "generated_config.hpp"         // for ENABLE_AS...
#include "instance.hpp"
#include "laggraph.hpp" // for LagGraph:...
#include "resource.hpp" // for ResVec
#include <algorithm>    // for move, max
#include <assert.h>     // for assert
#include <limits>       // for numeric_l...
#include <set>          // for set, allo...
#include <string>       // for operator+
#include <vector>       // for vector

Transformer::Transformer() {}

DummyStartEndTransformer::DummyStartEndTransformer() {}

Instance
DummyStartEndTransformer::get_transformed()
{
  // TODO hum… might be dangerous?
  return std::move(this->transformed);
}

TraitsRouter::transform_profile
DummyStartEndTransformer::get_requirements() const
{
  return TraitsRouter::transform_profile(
      {// static const unsigned long NO_LAGS                    ;
       TraitsRouter::WANT_MAYBE,
       // static const unsigned long LAGS_ONLY_SUCCESSORS       ;
       TraitsRouter::WANT_MAYBE,
       // static const unsigned long LAGS_ONLY_GREATER_DURATION ;
       TraitsRouter::WANT_MAYBE,
       // static const unsigned long LAGS_ONLY_POSITIVE         ;
       TraitsRouter::WANT_MAYBE,
       // static const unsigned long LAGS_DAG                   ;
       TraitsRouter::WANT_YES,
       //
       // static const unsigned long COMMON_RELEASE             ;
       TraitsRouter::WANT_MAYBE,
       // static const unsigned long COMMON_DEADLINE            ;
       TraitsRouter::WANT_MAYBE,
       // static const unsigned long COMMON_DURATION            ;
       TraitsRouter::WANT_MAYBE,

       // static const unsigned long CONSISTENT_WINDOWS         ;
       TraitsRouter::WANT_MAYBE,

       // static const unsigned long DUMMY_START_END            ;
       TraitsRouter::WANT_NO,

       // static const unsigned long NO_DRAIN                   ;
       TraitsRouter::WANT_MAYBE,

       // static const unsigned long NO_WINDOW_EXTENSION        ;
       TraitsRouter::WANT_YES,
       // static const unsigned long WINDOW_EXTENSION_JOBS_UNLIMITED    ;
       TraitsRouter::WANT_MAYBE

      });
}

TraitsRouter::transform_profile
DummyStartEndTransformer::get_transformation() const
{
  return TraitsRouter::transform_profile(
      {// static const unsigned long NO_LAGS                    ;
       TraitsRouter::WANT_NO,
       // static const unsigned long LAGS_ONLY_SUCCESSORS       ;
       TraitsRouter::WANT_MAYBE,
       // static const unsigned long LAGS_ONLY_GREATER_DURATION ;
       TraitsRouter::WANT_MAYBE,
       // static const unsigned long LAGS_ONLY_POSITIVE         ;
       TraitsRouter::WANT_MAYBE,
       // static const unsigned long LAGS_DAG                   ;
       TraitsRouter::WANT_MAYBE,

       // static const unsigned long COMMON_RELEASE             ;
       TraitsRouter::WANT_MAYBE,
       // static const unsigned long COMMON_DEADLINE            ;
       TraitsRouter::WANT_MAYBE,
       // static const unsigned long COMMON_DURATION            ;
       TraitsRouter::WANT_NO,

       // static const unsigned long CONSISTENT_WINDOWS         ;
       TraitsRouter::WANT_MAYBE,

       // static const unsigned long DUMMY_START_END            ;
       TraitsRouter::WANT_YES,

       // static const unsigned long NO_DRAIN                   ;
       TraitsRouter::WANT_MAYBE,

       // static const unsigned long NO_WINDOW_EXTENSION        ;
       TraitsRouter::WANT_YES,
       // static const unsigned long WINDOW_EXTENSION_JOBS_UNLIMITED    ;
       TraitsRouter::WANT_MAYBE});
}

void
DummyStartEndTransformer::run(const Instance & input)
{
  unsigned int max_deadline = std::numeric_limits<unsigned int>::min();
  unsigned int min_release = std::numeric_limits<unsigned int>::max();

  Traits new_traits = input.get_traits();
  new_traits.add_flag(Traits::DUMMY_START_END);
  new_traits.remove_flag(Traits::NO_LAGS);
  new_traits.remove_flag(Traits::COMMON_DURATION);

  this->transformed = Instance(input.get_id(), new_traits);

  for (unsigned int i = 0; i < input.job_count(); ++i) {
    max_deadline = std::max(max_deadline, input.get_job(i).get_deadline());
    min_release = std::min(min_release, input.get_job(i).get_release());
  }

  // TODO there should be a "deepcopy" for instances

  // First, copy all the jobs
  for (unsigned int i = 0; i < input.job_count(); ++i) {
    const Job & in_job = input.get_job(i);
    Job out_job = in_job;
    unsigned int new_id = this->transformed.add_job(std::move(out_job));
#ifdef ENABLE_ASSERTIONS
    assert(new_id == i);
#endif
  }

  // Now, copy all the resources
  for (unsigned int i = 0; i < input.resource_count(); ++i) {
    Resource out_res = input.get_resource(i); // copy s.t. it can be moved
    unsigned int new_id = this->transformed.add_resource(std::move(out_res));
    assert(new_id == i);
  }

  // Copy all time lags

  for (auto edge : input.get_laggraph().edges()) {
    unsigned int s = edge.s;
    unsigned int t = edge.t;
    const Job & s_job = this->transformed.get_job(s);
    const Job & t_job = this->transformed.get_job(t);

    this->transformed.get_laggraph().add_edge(
        s_job, t_job, {edge.lag, edge.drain_factor, edge.max_recharge});
  }

  /*
    for (unsigned int i = 0; i < this->transformed.job_count(); ++i) {
      const Job & s = this->transformed.get_job(i);
      for (unsigned int j = 0; i < this->transformed.job_count(); ++i) {
        const Job & t = this->transformed.get_job(j);

        // FIXME Uh! This only works if the IDs are equal
        if (input.get_laggraph().get_edge(s, t) != nullptr) {
          const auto *e = input.get_laggraph().get_edge(s, t);

        }
      }
    }
    */

  // Now, add dummy start / end
  ResVec zero_usages(input.resource_count(), 0);
  Job dummy_start_job = Job(min_release, max_deadline + 1, 0, zero_usages, 0);
  unsigned int dummy_start =
      this->transformed.add_job(std::move(dummy_start_job));
  dummy_start_job = this->transformed.get_job(dummy_start);

  Job dummy_end_job = Job(min_release, max_deadline + 1, 0, zero_usages, 0);
  unsigned int dummy_end = this->transformed.add_job(std::move(dummy_end_job));
  dummy_end_job = this->transformed.get_job(dummy_end);

  std::set<unsigned int> has_incoming_edges;
  std::set<unsigned int> has_outgoing_edges;

  for (auto & edge : input.get_laggraph().edges()) {
    has_incoming_edges.insert(edge.t);
    has_outgoing_edges.insert(edge.s);
  }

  for (unsigned int j = 0; j < input.job_count(); ++j) {
    assert(j < dummy_start);

    if (has_incoming_edges.find(j) == has_incoming_edges.end()) {
      this->transformed.get_laggraph().add_edge(
          dummy_start_job, this->transformed.get_job(j), {0, 0.0, 0});
    }

    if (has_outgoing_edges.find(j) == has_outgoing_edges.end()) {
      this->transformed.get_laggraph().add_edge(this->transformed.get_job(j),
                                                dummy_end_job, {0, 0.0, 0});
    }
  }
}

TraitsRouter::transform_profile
SameStartEndTransformer::get_requirements() const
{
  return TraitsRouter::transform_profile(
      {// static const unsigned long NO_LAGS                    ;
       TraitsRouter::WANT_MAYBE,
       // static const unsigned long LAGS_ONLY_SUCCESSORS       ;
       TraitsRouter::WANT_MAYBE,
       // static const unsigned long LAGS_ONLY_GREATER_DURATION ;
       TraitsRouter::WANT_MAYBE,
       // static const unsigned long LAGS_ONLY_POSITIVE         ;
       TraitsRouter::WANT_MAYBE,
       // static const unsigned long LAGS_DAG                   ;
       TraitsRouter::WANT_MAYBE,
       //
       // static const unsigned long COMMON_RELEASE             ;
       TraitsRouter::WANT_MAYBE,
       // static const unsigned long COMMON_DEADLINE            ;
       TraitsRouter::WANT_MAYBE,
       // static const unsigned long COMMON_DURATION            ;
       TraitsRouter::WANT_MAYBE,

       // static const unsigned long CONSISTENT_WINDOWS         ;
       TraitsRouter::WANT_MAYBE,

       // static const unsigned long DUMMY_START_END            ;
       TraitsRouter::WANT_MAYBE,

       // static const unsigned long NO_DRAIN                   ;
       TraitsRouter::WANT_MAYBE,

       // static const unsigned long NO_WINDOW_EXTENSION        ;
       TraitsRouter::WANT_YES,
       // static const unsigned long WINDOW_EXTENSION_JOBS_UNLIMITED    ;
       TraitsRouter::WANT_MAYBE});
}

TraitsRouter::transform_profile
SameStartEndTransformer::get_transformation() const
{
  return TraitsRouter::transform_profile(
      {// static const unsigned long NO_LAGS                    ;
       TraitsRouter::WANT_NO,
       // static const unsigned long LAGS_ONLY_SUCCESSORS       ;
       TraitsRouter::WANT_MAYBE,
       // static const unsigned long LAGS_ONLY_GREATER_DURATION ;
       TraitsRouter::WANT_MAYBE,
       // static const unsigned long LAGS_ONLY_POSITIVE         ;
       TraitsRouter::WANT_MAYBE,
       // static const unsigned long LAGS_DAG                   ;
       TraitsRouter::WANT_MAYBE,
       //
       // static const unsigned long COMMON_RELEASE             ;
       TraitsRouter::WANT_YES,
       // static const unsigned long COMMON_DEADLINE            ;
       TraitsRouter::WANT_YES,
       // static const unsigned long COMMON_DURATION            ;
       TraitsRouter::WANT_NO,

       // static const unsigned long CONSISTENT_WINDOWS         ;
       TraitsRouter::WANT_MAYBE,

       // static const unsigned long DUMMY_START_END            ;
       TraitsRouter::WANT_NO,

       // static const unsigned long NO_DRAIN                   ;
       TraitsRouter::WANT_MAYBE,

       // static const unsigned long NO_WINDOW_EXTENSION        ;
       TraitsRouter::WANT_YES,
       // static const unsigned long WINDOW_EXTENSION_JOBS_UNLIMITED    ;
       TraitsRouter::WANT_MAYBE});
}

Instance
SameStartEndTransformer::get_transformed()
{
  // TODO hum… might be dangerous?
  return std::move(this->transformed);
}

SameStartEndTransformer::SameStartEndTransformer() {}

void
SameStartEndTransformer::run(const Instance & input)
{
  Traits new_traits = input.get_traits();
  new_traits.add_flag(Traits::COMMON_RELEASE);
  new_traits.add_flag(Traits::COMMON_DEADLINE);
  new_traits.remove_flag(Traits::NO_LAGS);

  this->transformed = Instance(input.get_id(), new_traits);

  unsigned int max_deadline = std::numeric_limits<unsigned int>::min();
  unsigned int min_release = std::numeric_limits<unsigned int>::max();

  for (unsigned int i = 0; i < input.job_count(); ++i) {
    max_deadline = std::max(max_deadline, input.get_job(i).get_deadline());
    min_release = std::min(min_release, input.get_job(i).get_release());
  }

  // First, copy all the jobs, but set deadline and release accordingly
  for (unsigned int i = 0; i < input.job_count(); ++i) {
    const Job & in_job = input.get_job(i);
    Job out_job(min_release, max_deadline, in_job);
    unsigned int new_id = this->transformed.add_job(std::move(out_job));
    assert(new_id == i);
  }

  // Now, copy all the resources
  for (unsigned int i = 0; i < input.resource_count(); ++i) {
    Resource out_res = input.get_resource(i); // copy s.t. it can be moved
    unsigned int new_id = this->transformed.add_resource(std::move(out_res));
    assert(new_id == i);
  }

  // Copy all time lags
  for (auto edge : input.get_laggraph().edges()) {
    const Job & s = this->transformed.get_job(edge.s);
    const Job & t = this->transformed.get_job(edge.t);

    this->transformed.get_laggraph().add_edge(
        s, t, {edge.lag, edge.drain_factor, edge.max_recharge});
  }

  /*
    for (unsigned int i = 0; i < this->transformed.job_count(); ++i) {
      const Job & s = this->transformed.get_job(i);
      for (unsigned int j = 0; i < this->transformed.job_count(); ++i) {
        const Job & t = this->transformed.get_job(j);

        // FIXME Uh! This only works if the IDs are equal
        if (input.get_laggraph().get_edge(s, t) != nullptr) {
          const auto *e = input.get_laggraph().get_edge(s, t);
          this->transformed.get_laggraph().add_edge(s, t, *e);
        }
      }
    }
  */
  // Insert dummy jobs
  for (unsigned int i = 0; i < input.job_count(); ++i) {
    const Job & in_job = input.get_job(i);

    unsigned int orig_release = in_job.get_release();
    unsigned int orig_deadline = in_job.get_deadline();

    // Dummy ID 0 will be set by add_job()
    Job preprend_dummy(min_release, max_deadline, (orig_release - min_release),
                       ResVec(this->transformed.resource_count(), 0.0), 0);
    Job append_dummy(min_release, max_deadline, (max_deadline - orig_deadline),
                     ResVec(this->transformed.resource_count(), 0.0), 0);

    unsigned int prepend_index =
        this->transformed.add_job(std::move(preprend_dummy));
    unsigned int append_index =
        this->transformed.add_job(std::move(append_dummy));

    const Job & out_job = this->transformed.get_job(i);

    int forward_lag = 0;
    int reverse_lag = (int)in_job.get_duration();
    this->transformed.get_laggraph().add_edge(
        this->transformed.get_job(prepend_index), out_job,
        {forward_lag, 0.0, 0});
    this->transformed.get_laggraph().add_edge(
        out_job, this->transformed.get_job(append_index),
        {reverse_lag, 0.0, 0});
  }
}

const TransformerManager &
TransformerManager::get()
{
  static TransformerManager instance;
  return instance;
}

std::set<Transformer *> TransformerManager::all_transformers = {
    new SameStartEndTransformer(), new DummyStartEndTransformer(),
    new ConsistentWindowTransformer()};

const std::set<Transformer *>
TransformerManager::get_all() const
{
  return TransformerManager::all_transformers;
}

TransformerManager::TransformerManager() {}

TransformerManager::~TransformerManager() {}

ConsistentWindowTransformer::ConsistentWindowTransformer() {}

Instance
ConsistentWindowTransformer::get_transformed()
{
  // TODO hum… might be dangerous?
  return std::move(this->transformed);
}

TraitsRouter::transform_profile
ConsistentWindowTransformer::get_requirements() const
{
  return TraitsRouter::transform_profile(
      {// static const unsigned long NO_LAGS                    ;
       TraitsRouter::WANT_MAYBE,
       // static const unsigned long LAGS_ONLY_SUCCESSORS       ;
       TraitsRouter::WANT_MAYBE,
       // static const unsigned long LAGS_ONLY_GREATER_DURATION ;
       TraitsRouter::WANT_MAYBE,
       // static const unsigned long LAGS_ONLY_POSITIVE         ;
       TraitsRouter::WANT_MAYBE,
       // static const unsigned long LAGS_DAG                   ;
       TraitsRouter::WANT_MAYBE,
       //
       // static const unsigned long COMMON_RELEASE             ;
       TraitsRouter::WANT_MAYBE,
       // static const unsigned long COMMON_DEADLINE            ;
       TraitsRouter::WANT_MAYBE,
       // static const unsigned long COMMON_DURATION            ;
       TraitsRouter::WANT_MAYBE,

       // static const unsigned long CONSISTENT_WINDOWS         ;
       TraitsRouter::WANT_MAYBE,

       // static const unsigned long DUMMY_START_END            ;
       TraitsRouter::WANT_MAYBE,

       // static const unsigned long NO_DRAIN                   ;
       TraitsRouter::WANT_MAYBE,

       // static const unsigned long NO_WINDOW_EXTENSION        ;
       TraitsRouter::WANT_YES,
       // static const unsigned long WINDOW_EXTENSION_JOBS_UNLIMITED    ;
       TraitsRouter::WANT_MAYBE});
}

TraitsRouter::transform_profile
ConsistentWindowTransformer::get_transformation() const
{
  return TraitsRouter::transform_profile(
      {// static const unsigned long NO_LAGS                    ;
       TraitsRouter::WANT_MAYBE,
       // static const unsigned long LAGS_ONLY_SUCCESSORS       ;
       TraitsRouter::WANT_MAYBE,
       // static const unsigned long LAGS_ONLY_GREATER_DURATION ;
       TraitsRouter::WANT_MAYBE,
       // static const unsigned long LAGS_ONLY_POSITIVE         ;
       TraitsRouter::WANT_MAYBE,
       // static const unsigned long LAGS_DAG                   ;
       TraitsRouter::WANT_MAYBE,

       // static const unsigned long COMMON_RELEASE             ;
       TraitsRouter::WANT_NO,
       // static const unsigned long COMMON_DEADLINE            ;
       TraitsRouter::WANT_NO,
       // static const unsigned long COMMON_DURATION            ;
       TraitsRouter::WANT_MAYBE,

       // static const unsigned long CONSISTENT_WINDOWS         ;
       TraitsRouter::WANT_YES,

       // static const unsigned long DUMMY_START_END            ;
       TraitsRouter::WANT_MAYBE,

       // static const unsigned long NO_DRAIN                   ;
       TraitsRouter::WANT_MAYBE,

       // static const unsigned long NO_WINDOW_EXTENSION        ;
       TraitsRouter::WANT_YES,
       // static const unsigned long WINDOW_EXTENSION_JOBS_UNLIMITED    ;
       TraitsRouter::WANT_MAYBE});
}

void
ConsistentWindowTransformer::run(const Instance & input)
{
  CriticalPathComputer cpc(input);

  Traits new_traits = input.get_traits();
  new_traits.add_flag(Traits::CONSISTENT_WINDOWS);
  new_traits.remove_flag(Traits::COMMON_DEADLINE);
  new_traits.remove_flag(Traits::COMMON_RELEASE);

  this->transformed = Instance(input.get_id(), new_traits);

  std::vector<unsigned int> earliest_starts = cpc.get_forward();
  std::vector<unsigned int> latest_finishs = cpc.get_reverse();

  // First, copy all the jobs with new deadline / release
  for (unsigned int i = 0; i < input.job_count(); ++i) {
    const Job & in_job = input.get_job(i);

    unsigned int new_release =
        std::max(in_job.get_release(), earliest_starts[in_job.get_jid()]);
    unsigned int new_deadline =
        std::min(in_job.get_deadline(), latest_finishs[in_job.get_jid()]);

    if (new_release + in_job.get_duration() > new_deadline) {
      throw InconsistentDataError(input, -1, FAULT_CRITICAL_PATH_INFEASIBLE,
                                  "The critical paths for job " +
                                      std::to_string(in_job.get_jid()) +
                                      " are infeasible");
    }

    Job out_job = Job(new_release, new_deadline, in_job);

    unsigned int new_id = this->transformed.add_job(std::move(out_job));
#ifdef ENABLE_ASSERTIONS
    assert(new_id == i);
#endif
  }

  // Now, copy all the resources
  for (unsigned int i = 0; i < input.resource_count(); ++i) {
    Resource out_res = input.get_resource(i); // copy s.t. it can be moved
    unsigned int new_id = this->transformed.add_resource(std::move(out_res));
    assert(new_id == i);
  }

  // Copy all time lags

  for (auto edge : input.get_laggraph().edges()) {
    unsigned int s = edge.s;
    unsigned int t = edge.t;
    const Job & s_job = this->transformed.get_job(s);
    const Job & t_job = this->transformed.get_job(t);

    this->transformed.get_laggraph().add_edge(
        s_job, t_job, {edge.lag, edge.drain_factor, edge.max_recharge});
  }
}
