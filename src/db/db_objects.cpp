//
// Created by lukas on 24.10.17.
//

#include "db_objects.hpp"

#include "../util/git.hpp"

#include <ctime>
#include <memory>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"

DBInvocation::DBInvocation(std::string cmdline, std::string git_revision,
                           std::string hostname, unsigned long time)
    : cmdline(cmdline), git_revision(git_revision), hostname(hostname),
      time(time)
{}

DBInvocation::DBInvocation(std::shared_ptr<const DBInvocation> src)
    : cmdline(src->cmdline), git_revision(src->git_revision),
      hostname(src->hostname), time(src->time)
{}

unsigned long
DBInvocation::get_id() const noexcept
{
  return this->id_;
}

DBResult::DBResult(std::string run, std::string instance, double score,
                   std::string algorithm, std::string config_name, int seed,
                   bool optimal, bool feasible, Maybe<double> lower_bound,
                   double elapsed, std::shared_ptr<DBConfig> cfg,
                   std::shared_ptr<DBInvocation> invocation)
    : run(run), instance(instance), score(score), algorithm(algorithm),
      config(config_name), seed(seed), optimal(optimal), feasible(feasible),
      elapsed(elapsed), time((unsigned long)std::time(nullptr)),
      invocation(invocation), cfg(cfg)
{
  if (lower_bound.valid()) {
    this->lower_bound.reset(new double(lower_bound.value()));
  }
}

DBResult::DBResult(std::shared_ptr<const DBResult> src)
    : run(src->run), instance(src->instance), score(src->score),
      algorithm(src->algorithm), config(src->config), seed(src->seed),
      optimal(src->optimal), feasible(src->feasible),
      lower_bound(src->lower_bound), elapsed(src->elapsed), time(src->time)
{}

DBConfig::DBConfig(const std::string & name, Maybe<unsigned int> time_limit)
    : name(name)
{
  if (time_limit.valid()) {
    this->time_limit.reset(new unsigned int(time_limit.value()));
  }
}

DBConfig::DBConfig(std::shared_ptr<const DBConfig> src)
    : name(src->name), time_limit(src->time_limit)
{}

DBConfigKV::DBConfigKV(std::shared_ptr<DBConfig> cfg, const std::string & key,
                       const std::string & value)
    : cfg(cfg), key(key), value(value)
{}

DBConfigKV::DBConfigKV(std::shared_ptr<const DBConfigKV> src)
    : key(src->key), value(src->value)
{}

DBSolution::DBSolution(std::shared_ptr<DBResult> res) : res(res) {}

DBSolution::DBSolution(std::shared_ptr<const DBSolution> src)
{
  (void)src;
}

DBResourcesInfo::DBResourcesInfo(std::shared_ptr<DBResult> res_in,
                                 odb::nullable<size_t> max_rss_size_in,
                                 odb::nullable<size_t> max_data_size_in,
                                 odb::nullable<size_t> malloc_max_size_in,
                                 odb::nullable<size_t> malloc_count_in,
                                 size_t major_pagefaults_in,
                                 size_t minor_pagefaults_in,
                                 unsigned long user_usecs_in,
                                 unsigned long system_usecs_in)
    : res(res_in), major_pagefaults(major_pagefaults_in),
      minor_pagefaults(minor_pagefaults_in), user_usecs(user_usecs_in),
      system_usecs(system_usecs_in), max_rss_size(max_rss_size_in),
      max_data_size(max_data_size_in), malloc_max_size(malloc_max_size_in),
      malloc_count(malloc_count_in)
{}

DBResourcesInfo::DBResourcesInfo(std::shared_ptr<const DBResourcesInfo> src)
    : major_pagefaults(src->major_pagefaults),
      minor_pagefaults(src->minor_pagefaults), user_usecs(src->user_usecs),
      system_usecs(src->system_usecs), max_rss_size(src->max_rss_size),
      max_data_size(src->max_data_size), malloc_max_size(src->malloc_max_size),
      malloc_count(src->malloc_count)
{}

DBPapiMeasurement::DBPapiMeasurement(std::shared_ptr<DBResult> res_in,
                                     std::string event_type_in,
                                     long long event_count_in)
    : res(res_in), event_type(event_type_in), event_count(event_count_in)
{}

DBPapiMeasurement::DBPapiMeasurement(
    std::shared_ptr<const DBPapiMeasurement> src)
    : event_type(src->event_type), event_count(src->event_count)
{}

DBSolutionJob::DBSolutionJob(std::shared_ptr<DBSolution> sol,
                             unsigned int job_id, unsigned int start_time)
    : sol(sol), job_id(job_id), start_time(start_time)
{}

DBSolutionJob::DBSolutionJob(std::shared_ptr<const DBSolutionJob> src)
    : job_id(src->job_id), start_time(src->start_time)
{}

DBIntermediate::DBIntermediate(std::shared_ptr<DBResult> res,
                               Maybe<double> time,
                               Maybe<unsigned int> iteration,
                               Maybe<double> costs, Maybe<double> bound,
                               std::shared_ptr<DBSolution> solution)
    : res(res), solution(solution)
{
  if (time.valid()) {
    this->time.reset(new double(time.value()));
  }
  if (iteration.valid()) {
    this->iteration.reset(new unsigned int(iteration.value()));
  }
  if (costs.valid()) {
    this->costs.reset(new double(costs.value()));
  }
  if (bound.valid()) {
    this->bound.reset(new double(bound.value()));
  }
}

DBIntermediate::DBIntermediate(std::shared_ptr<const DBIntermediate> src)
    : time(src->time), iteration(src->iteration), costs(src->costs),
      bound(src->bound)
{}

DBError::DBError(unsigned long timestamp, std::string run, std::string instance,
                 std::string algorithm, std::string config_name, int seed,
                 int fault_code, int error_id)
    : timestamp(timestamp), run(run), instance(instance), algorithm(algorithm),
      config(config_name), seed(seed), fault_code(fault_code),
      error_id(error_id)
{}

DBError::DBError(std::shared_ptr<const DBError> src)
    : timestamp(src->timestamp), run(src->run), instance(src->instance),
      algorithm(src->algorithm), config(src->config), seed(src->seed),
      fault_code(src->fault_code), error_id(src->error_id)
{}

DBExtendedMeasure::DBExtendedMeasure(std::shared_ptr<DBResult> res,
                                     std::string key,
                                     Maybe<unsigned int> iteration,
                                     Maybe<double> time, Maybe<int> v_int,
                                     Maybe<double> v_double)
    : res(res), key(key)
{
  if (iteration.valid()) {
    this->iteration.reset(new unsigned int(iteration.value()));
  }
  if (time.valid()) {
    this->time.reset(new double(time.value()));
  }
  if (v_int.valid()) {
    this->v_int.reset(new int(v_int.value()));
  }
  if (v_double.valid()) {
    this->v_double.reset(new double(v_double.value()));
  }
}

DBExtendedMeasure::DBExtendedMeasure(
    std::shared_ptr<const DBExtendedMeasure> src)
    : key(src->key), iteration(src->iteration), time(src->time),
      v_int(src->v_int), v_double(src->v_double)
{}

#pragma GCC diagnostic pop
