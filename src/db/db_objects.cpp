//
// Created by lukas on 24.10.17.
//

#include "db_objects.hpp"

#include <memory>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"

DBResult::DBResult(std::string run, std::string instance, double score, std::string algorithm, std::string config_name, int seed,
                   bool optimal, bool feasible, Maybe<double> lower_bound, double elapsed,
                   std::shared_ptr<DBConfig> cfg)
    : run(run), instance(instance), score(score), algorithm(algorithm), config(config_name), seed(seed),
      optimal(optimal), feasible(feasible), elapsed(elapsed), cfg(cfg)
{
    if (lower_bound.valid()) {
        this->lower_bound.reset(new double(lower_bound.value()));
    }
}

DBConfig::DBConfig(const std::string & name, Maybe<unsigned int> time_limit)
    : name(name)
{
    if (time_limit.valid()) {
        this->time_limit.reset(new unsigned int(time_limit.value()));
    }
}

DBConfigKV::DBConfigKV(std::shared_ptr<DBConfig> cfg, const std::string & key,
                       const std::string & value)
  : cfg(cfg), key(key), value(value)
{}

DBSolution::DBSolution(std::shared_ptr<DBResult> res)
    : res(res)
{}

DBSolutionJob::DBSolutionJob(std::shared_ptr<DBSolution> sol, unsigned int job_id,
                             unsigned int start_time)
    : sol(sol), job_id(job_id), start_time(start_time)
{}

DBIntermediate::DBIntermediate(std::shared_ptr<DBResult> res, Maybe<double> time,
                               Maybe<unsigned int> iteration, Maybe<double> costs,
                               Maybe<double> bound, std::shared_ptr<DBSolution> solution)
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

DBError::DBError(unsigned long timestamp, std::string run, std::string instance,
                 std::string algorithm, std::string config_name, int seed, int fault_code, int error_id)
    : timestamp(timestamp), run(run), instance(instance), algorithm(algorithm), config(config_name), seed(seed),
      fault_code(fault_code), error_id(error_id)
{}

DBExtendedMeasure::DBExtendedMeasure(std::shared_ptr<DBResult> res, std::string key, Maybe<unsigned int> iteration,
                                     Maybe<double> time, Maybe<int> v_int, Maybe<double> v_double)
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

#pragma GCC diagnostic pop
