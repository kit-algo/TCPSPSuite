//
// Created by lukas on 24.10.17.
//

#ifndef TCPSPSUITE_STORAGE_HPP
#define TCPSPSUITE_STORAGE_HPP

#include <mutex>
#include <memory>                          // for shared_ptr, unique_ptr
#include <string>                          // for string
#include <vector>                          // for vector
#include "../instance/solution.hpp"        // for Solution
#include "../util/log.hpp"  // for Log
#include "../datastructures/maybe.hpp"  // for Maybe
#include <odb/database.hxx>

class DBConfig;
class DBResult;
class DBSolution;
class SolverConfig;

class AdditionalResultStorage {
public:
    // We never ever want to copy this. Results must be stored in the created instance!
    AdditionalResultStorage() = default;
    AdditionalResultStorage(const AdditionalResultStorage &) = delete;
    AdditionalResultStorage & operator=(const AdditionalResultStorage &) = delete;

    struct IntermediateResult {
    public:
        Maybe<double> time;
        Maybe<unsigned int> iteration;
        Maybe<double> costs;
        Maybe<double> bound;
        Maybe<Solution> solution;
    };

    struct ExtendedMeasure {
    public:
        //const static unsigned int   TYPE_STRING;
        constexpr static unsigned int   TYPE_DOUBLE = 2;
        constexpr static unsigned int   TYPE_INT = 3;

        std::string key;
        Maybe<unsigned int> iteration;
        Maybe<double> time;

        unsigned int type;
        union V {
            //std::string   v_string;
            double        v_double;
            int             v_int;
            V(double v) : v_double(v) {};
            V(int v) : v_int(v) {};
        } value;
    };

    std::vector<IntermediateResult> intermediate_results;
    std::vector<ExtendedMeasure> extended_measures;
};

class Storage
{
public:
    explicit Storage(std::string filename);

    long unsigned int
    insert(const Solution & sol, const std::string & run_id, const std::string & algorithm_id,
           const std::string & config_name, int instance_seed, double elapsed_time, const SolverConfig & sc,
           const AdditionalResultStorage & additional);

    void insert_error(const std::string & instance_id, const std::string & run_id,
                      const std::string & algorithm_id, const std::string & config_name, int seed,
                      unsigned int error_id, int fault_code);

    bool check_result(const std::string & instance_id, const std::string & run_id,
                      const std::string & algorithm_id, const std::string & config_name,
                      bool only_optimal = false);
private:
    std::mutex insert_mutex;
    std::mutex insert_error_mutex;
    std::mutex check_result_mutex;

    void insert_intermediate_result(std::shared_ptr<DBResult> res,
                                    const AdditionalResultStorage::IntermediateResult &intermediate);
    void insert_extended_measure(std::shared_ptr<DBResult> res,
                                 const AdditionalResultStorage::ExtendedMeasure &measure);
    std::shared_ptr<DBSolution> insert_solution(std::shared_ptr<DBResult> res, const Solution & sol);

		std::unique_ptr<odb::core::database> db;

    std::shared_ptr<DBConfig> get_solverconfig(const SolverConfig & sc);
    std::shared_ptr<DBConfig> get_or_insert_solverconfig(const SolverConfig & sc);

    Log l;
};


#endif //TCPSPSUITE_STORAGE_HPP
