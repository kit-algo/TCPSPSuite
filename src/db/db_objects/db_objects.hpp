//
// Created by lukas on 24.10.17.
//

#ifndef TCPSPSUITE_DB_OBJECTS_HPP
#define TCPSPSUITE_DB_OBJECTS_HPP

#include <string>
#include <memory>
#include <vector>
#include <odb/core.hxx>
#include <odb/lazy-ptr.hxx>

#include "../../datastructures/maybe.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"

// Forwards for back-and-forth relationships
class DBIntermediate;
class DBSolutionJob;
class DBConfig;

#pragma db object
class DBConfigKV {
public:
#pragma db not_null
    std::shared_ptr<DBConfig> cfg;
    std::string key;
    std::string value;

    DBConfigKV(std::shared_ptr<DBConfig> cfg, const std::string & key, const std::string & value);
private:
    DBConfigKV() {}

#pragma db id auto
    unsigned long id_;

    friend class odb::access;
};

#pragma db object
class DBConfig {
public:
    #pragma db unique
    std::string           name;

    #pragma db null
    std::shared_ptr<unsigned int>   time_limit;

    #pragma db value_not_null inverse(cfg)
    std::vector<odb::core::lazy_weak_ptr<DBConfigKV>> entries;

    DBConfig(const std::string & name, Maybe<unsigned int> time_limit);

private:
    DBConfig() {}

#pragma db id auto
    unsigned long id_;

    friend class odb::access;
};

#pragma db object
class DBResult {
public:
    DBResult(std::string run, std::string instance, double score, std::string algorithm, std::string config_name, int seed,
             bool optimal, bool feasible, Maybe<double> lower_bound, double elapsed,
             std::shared_ptr<DBConfig> cfg);

    std::string               run;
    std::string               instance;
    double                    score;
    std::string               algorithm;
    std::string               config;
    int                       seed;
    bool                      optimal;
    bool                      feasible;
    #pragma db null
    std::shared_ptr<double>   lower_bound;
    double                    elapsed;

#pragma db not_null
    std::shared_ptr<DBConfig>   cfg;

private:
    DBResult() {}

#pragma db id auto
    unsigned long id_;

    friend class odb::access;
};

#pragma db object
class DBSolution {
public:
    DBSolution(std::shared_ptr<DBResult> res);
#pragma db not_null
    std::shared_ptr<DBResult> res;

#pragma db value_not_null inverse(sol)
    std::vector<std::shared_ptr<DBSolutionJob>> jobs;

private:
    DBSolution() {}

#pragma db id auto
    unsigned long id_;

    friend class odb::access;
};

#pragma db object
class DBSolutionJob {
public:
    DBSolutionJob(std::shared_ptr<DBSolution> sol, unsigned int job_id, unsigned int start_time);

#pragma db not_null
    std::shared_ptr<DBSolution> sol;

    unsigned int job_id;
    unsigned int start_time;

private:
    DBSolutionJob() {}

#pragma db id auto
    unsigned long id_;

    friend class odb::access;
};

#pragma db object
class DBIntermediate
{
public:
    DBIntermediate(std::shared_ptr<DBResult> res, Maybe<double> time, Maybe<unsigned int> iteration,
                   Maybe<double> costs, Maybe<double> bound, std::shared_ptr<DBSolution> solution);

  #pragma db not_null
    std::shared_ptr<DBResult> res;

    std::shared_ptr<double> time;
    std::shared_ptr<unsigned int> iteration;
    std::shared_ptr<double> costs;
    std::shared_ptr<double> bound;

    std::shared_ptr<DBSolution> solution;
private:
    DBIntermediate() {}

#pragma db id auto
    unsigned long id_;

    friend class odb::access;
};

#pragma db object
class DBError {
public:
    DBError(unsigned long timestamp, std::string run, std::string instance, std::string algorithm,
            std::string config_name, int seed, int fault_code, int error_id);
    unsigned long timestamp;
    std::string run;
    std::string instance;
    std::string algorithm;
    std::string config;
    int seed;
    int fault_code;
    int error_id;

private:
    DBError() {}

#pragma db id auto
    unsigned long id_;

    friend class odb::access;
};

#pragma db object
class DBExtendedMeasure {
public:
    DBExtendedMeasure(std::shared_ptr<DBResult> res, std::string key, Maybe<unsigned int> iteration,
                      Maybe<double> time, Maybe<int> v_int, Maybe<double> v_double);

  #pragma db not_null
    std::shared_ptr<DBResult> res;

    std::string key;
    #pragma db null
    std::shared_ptr<unsigned int> iteration;
    #pragma db null
    std::shared_ptr<double> time;
    #pragma db null
    std::shared_ptr<int> v_int;
    #pragma db null
    std::shared_ptr<double> v_double;

private:
    DBExtendedMeasure() {}

#pragma db id auto
    unsigned long id_;

    friend class odb::access;
};

#pragma GCC diagnostic pop

#endif //TCPSPSUITE_DB_OBJECTS_HPP
