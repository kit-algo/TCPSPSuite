//
// Created by lukas on 24.10.17.
//

#ifndef TCPSPSUITE_STORAGE_HPP
#define TCPSPSUITE_STORAGE_HPP

#include "../datastructures/maybe.hpp" // for Maybe
#include "../instance/solution.hpp"    // for Solution
#include "../manager/memoryinfo.hpp"
#include "../util/log.hpp" // for Log
#include "db_factory.hpp"

#include <memory> // for shared_ptr, unique_ptr
#include <mutex>
#include <odb/database.hxx>
#include <string> // for string
#include <vector> // for vector

class DBConfig;
class DBResult;
class DBSolution;
class SolverConfig;
class DBMerger;
class DBInvocation;

class AdditionalResultStorage {
public:
	// We never ever want to copy this. Results must be stored in the created
	// instance!
	AdditionalResultStorage() = default;
	AdditionalResultStorage(const AdditionalResultStorage &) = delete;
	AdditionalResultStorage & operator=(const AdditionalResultStorage &) = delete;

	struct IntermediateResult
	{
	public:
		Maybe<double> time;
		Maybe<unsigned int> iteration;
		Maybe<double> costs;
		Maybe<double> bound;
		Maybe<Solution> solution;
	};

	struct ExtendedMeasure
	{
	public:
		// const static unsigned int   TYPE_STRING;
		constexpr static unsigned int TYPE_DOUBLE = 2;
		constexpr static unsigned int TYPE_INT = 3;

		std::string key;
		Maybe<unsigned int> iteration;
		Maybe<double> time;

		unsigned int type;
		union V {
			// std::string   v_string;
			double v_double;
			int v_int;
			V(double v) : v_double(v){};
			V(int v) : v_int(v){};
		} value;
	};

	std::vector<IntermediateResult> intermediate_results;
	std::vector<ExtendedMeasure> extended_measures;
};

class Storage {
public:
	explicit Storage(std::string filename, unsigned int retry_count = 1000);

	long unsigned int insert(const Solution & sol, const std::string & run_id,
	                         const std::string & algorithm_id,
	                         const std::string & config_name, int instance_seed,
	                         double elapsed_time, const SolverConfig & sc,
	                         const AdditionalResultStorage & additional,
	                         const manager::LinuxMemoryInfo * mem_info,
	                         const manager::PAPIPerformanceInfo * papi_info);

	void insert_error(const std::string & instance_id, const std::string & run_id,
	                  const std::string & algorithm_id,
	                  const std::string & config_name, int seed,
	                  unsigned int error_id, int fault_code);

	/*
	 * Querying
	 */
	bool check_result(const std::string & instance_id, const std::string & run_id,
	                  const std::string & algorithm_id, const SolverConfig & sc,
	                  bool only_optimal = false, bool ignore_config_name = false,
	                  bool ignore_run_name = false);

	bool check_error(std::vector<int> error_ids, std::vector<int> fault_codes,
	                 const std::string & instance_id, const std::string & run_id,
	                 const std::string & algorithm_id, const SolverConfig & sc,
	                 bool ignore_config_name = false,
	                 bool ignore_run_name = false);

	std::vector<std::shared_ptr<DBResult>>
	get_results_for_config(const SolverConfig & sc);

	std::shared_ptr<DBConfig>
	find_equivalent_config(std::shared_ptr<DBConfig> src);

	static void initialize(std::string filename, int argc, const char ** argv);
	static std::shared_ptr<DBInvocation> get_invocation();

private:
	static std::mutex insert_mutex;
	static std::mutex insert_error_mutex;
	static std::mutex check_result_mutex;
	static std::mutex check_error_mutex;

	static std::shared_ptr<DBInvocation> invocation;

	unsigned int retry_count;

	std::vector<unsigned long> find_db_configs(const SolverConfig & sc,
	                                           bool ignore_name = false);
	/*
	bool are_configs_equal(const SolverConfig & sc,
	                       std::shared_ptr<DBConfig> dbcfg) const;
	*/
	void insert_intermediate_result(
	    std::shared_ptr<DBResult> res,
	    const AdditionalResultStorage::IntermediateResult & intermediate);
	void insert_extended_measure(
	    std::shared_ptr<DBResult> res,
	    const AdditionalResultStorage::ExtendedMeasure & measure);
	std::shared_ptr<DBSolution> insert_solution(std::shared_ptr<DBResult> res,
	                                            const Solution & sol);

	std::unique_ptr<odb::database> db;

	std::shared_ptr<DBConfig> get_solverconfig(const SolverConfig & sc);
	std::shared_ptr<DBConfig> get_or_insert_solverconfig(const SolverConfig & sc);

	Log l;

	// The merger has raw access to the database
	friend class DBMerger;
};

#endif // TCPSPSUITE_STORAGE_HPP
