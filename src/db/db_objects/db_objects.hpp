//
// Created by lukas on 24.10.17.
//

#ifndef TCPSPSUITE_DB_OBJECTS_HPP
#define TCPSPSUITE_DB_OBJECTS_HPP

#include <iostream>
#include <memory>
#include <odb/core.hxx>
#include <odb/lazy-ptr.hxx>
#include <odb/nullable.hxx>
#include <string>
#include <vector>

#include <boost/date_time/posix_time/posix_time_types.hpp>

#include "../../datastructures/maybe.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"

// Forwards for back-and-forth relationships
class DBIntermediate;
class DBSolutionJob;
class DBConfig;
class DBResourcesInfo;
class DBPapiMeasurement;
class DBSolution;
class DBExtendedMeasure;
class DBResult;

#pragma db namespace() session
#pragma db namespace() pointer(std::shared_ptr)

#pragma db object
class DBConfigKV {
public:
#pragma db not_null
	std::weak_ptr<DBConfig> cfg;
	std::string key;
	std::string value;

	DBConfigKV(std::shared_ptr<DBConfig> cfg, const std::string & key,
	           const std::string & value);
	DBConfigKV(std::shared_ptr<const DBConfigKV> src);

#pragma db index member(cfg)

private:
	DBConfigKV() {}

#pragma db id auto
	unsigned long id_;

	friend class odb::access;
};

#pragma db object
class DBConfig {
public:
	std::string name;

#pragma db null
	std::shared_ptr<unsigned int> time_limit;

#pragma db value_not_null inverse(cfg)
	std::vector<std::shared_ptr<DBConfigKV>> entries;

	DBConfig(const std::string & name, Maybe<unsigned int> time_limit);
	DBConfig(std::shared_ptr<const DBConfig> src);

#pragma db index member(name)
private:
	DBConfig() {}

#pragma db id auto
	unsigned long id_;

	friend class odb::access;
};

#pragma db object
class DBInvocation {
public:
	DBInvocation(std::string cmdline, std::string git_revision,
	             std::string hostname, unsigned long time);
	DBInvocation(std::shared_ptr<const DBInvocation> src);

	std::string cmdline;
	std::string git_revision;
	std::string hostname;
	unsigned long time;

#pragma db value_not_null inverse(invocation)
	std::vector<std::shared_ptr<DBResult>> results;

	unsigned long get_id() const noexcept;

private:
	DBInvocation(){};

#pragma db id auto
	unsigned long id_;

	friend class odb::access;
};

#pragma db object
class DBResult {
public:
	DBResult(std::string run, std::string instance, double score,
	         std::string algorithm, std::string config_name, int seed,
	         bool optimal, bool feasible, Maybe<double> lower_bound,
	         double elapsed, std::shared_ptr<DBConfig> cfg,
	         std::shared_ptr<DBInvocation> invocation);
	DBResult(std::shared_ptr<const DBResult> src);

	std::string run;

	std::string instance;
	double score;
	std::string algorithm;
	std::string config; // TODO remove this
	int seed;
	bool optimal;
	bool feasible;
#pragma db null
	std::shared_ptr<double> lower_bound;
	double elapsed;
	unsigned long time;
	std::shared_ptr<DBInvocation> invocation;

#pragma db not_null
	std::shared_ptr<DBConfig> cfg;

	// Reverse sides of stuff that points here
#pragma db inverse(res)
	std::vector<std::shared_ptr<DBResourcesInfo>> resource_infos;
#pragma db inverse(res)
	std::vector<std::shared_ptr<DBPapiMeasurement>> papi_measurements;
#pragma db inverse(res)
	std::vector<std::shared_ptr<DBSolution>> solutions;
#pragma db inverse(res)
	std::vector<std::shared_ptr<DBIntermediate>> intermediates;
#pragma db inverse(res)
	std::vector<std::shared_ptr<DBExtendedMeasure>> extended_measures;

#pragma db index member(instance)
#pragma db index member(algorithm)
#pragma db index member(cfg)

	unsigned long
	get_id() const noexcept
	{
		return this->id_;
	}

private:
	DBResult() {}

#pragma db id auto
	unsigned long id_;

	friend class odb::access;
};

#pragma db object
class DBResourcesInfo {
public:
	DBResourcesInfo(std::shared_ptr<DBResult> res,
	                odb::nullable<size_t> max_rss_size,
	                odb::nullable<size_t> max_data_size,
	                odb::nullable<size_t> malloc_max_size,
	                odb::nullable<size_t> malloc_count, size_t major_pagefaults,
	                size_t minor_pagefaults, unsigned long user_usecs,
	                unsigned long system_usecs);
	DBResourcesInfo(std::shared_ptr<const DBResourcesInfo> src);

#pragma db not_null
	std::shared_ptr<DBResult> res;

	size_t major_pagefaults;
	size_t minor_pagefaults;
	unsigned long user_usecs;
	unsigned long system_usecs;

	odb::nullable<size_t> max_rss_size;
	odb::nullable<size_t> max_data_size;
	odb::nullable<size_t> malloc_max_size;
	odb::nullable<size_t> malloc_count;

#pragma db index member(res)

private:
	DBResourcesInfo() {}

#pragma db id auto
	unsigned long id_;

	friend class odb::access;
};

#pragma db object
class DBPapiMeasurement {
public:
	DBPapiMeasurement(std::shared_ptr<DBResult> res, std::string event_type,
	                  long long event_count);
	DBPapiMeasurement(std::shared_ptr<const DBPapiMeasurement> src);

#pragma db not_null
	std::shared_ptr<DBResult> res;

	std::string event_type;
	long long event_count;

#pragma db index member(res)

private:
	DBPapiMeasurement() {}

#pragma db id auto
	unsigned long id_;

	friend class odb::access;
};

#pragma db object
class DBSolution {
public:
	DBSolution(std::shared_ptr<DBResult> res);
	DBSolution(std::shared_ptr<const DBSolution> src);

#pragma db not_null
	std::shared_ptr<DBResult> res;

#pragma db value_not_null inverse(sol)
	std::vector<std::shared_ptr<DBSolutionJob>> jobs;
#pragma db inverse(solution)
	std::vector<std::shared_ptr<DBIntermediate>> intermediates;

#pragma db index member(res)

	unsigned long
	get_id() const
	{
		return this->id_;
	}

private:
	DBSolution() {}

#pragma db id auto
	unsigned long id_;

	friend class odb::access;
};

#pragma db object
class DBSolutionJob {
public:
	DBSolutionJob(std::shared_ptr<DBSolution> sol, unsigned int job_id,
	              unsigned int start_time);
	DBSolutionJob(std::shared_ptr<const DBSolutionJob> src);

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
class DBIntermediate {
public:
	DBIntermediate(std::shared_ptr<DBResult> res, Maybe<double> time,
	               Maybe<unsigned int> iteration, Maybe<double> costs,
	               Maybe<double> bound, std::shared_ptr<DBSolution> solution);
	DBIntermediate(std::shared_ptr<const DBIntermediate> src);

#pragma db not_null
	std::shared_ptr<DBResult> res;

	std::shared_ptr<double> time;
	std::shared_ptr<unsigned int> iteration;
	std::shared_ptr<double> costs;
	std::shared_ptr<double> bound;

	std::shared_ptr<DBSolution> solution;

	unsigned long
	get_id() const
	{
		return this->id_;
	}

#pragma db index member(res)

private:
	DBIntermediate() {}

#pragma db id auto
	unsigned long id_;

	friend class odb::access;
};

#pragma db object
class DBError {
public:
	// TODO add config
	DBError(unsigned long timestamp, std::string run, std::string instance,
	        std::string algorithm, std::string config_name, int seed,
	        int fault_code, int error_id);
	DBError(std::shared_ptr<const DBError> src);

	unsigned long timestamp;
	std::string run;
	std::string instance;
	std::string algorithm;
	std::string config;
	int seed;
	int fault_code;
	int error_id;
	unsigned long time;
	std::string git_revision;

private:
	DBError() {}

#pragma db id auto
	unsigned long id_;

	friend class odb::access;
};

#pragma db object
class DBExtendedMeasure {
public:
	DBExtendedMeasure(std::shared_ptr<DBResult> res, std::string key,
	                  Maybe<unsigned int> iteration, Maybe<double> time,
	                  Maybe<int> v_int, Maybe<double> v_double);
	DBExtendedMeasure(std::shared_ptr<const DBExtendedMeasure> src);

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

#pragma db index member(res)

private:
	DBExtendedMeasure() {}

#pragma db id auto
	unsigned long id_;

	friend class odb::access;
};

/*
 * Magic view to retrieve a specific config
 */
#pragma db view query("SELECT DBConfig.id from DBConfig INNER JOIN (")
struct ConfigGetterView
{
#pragma db type("INTEGER")
	unsigned long config_id;
};

#pragma GCC diagnostic pop

#endif // TCPSPSUITE_DB_OBJECTS_HPP
