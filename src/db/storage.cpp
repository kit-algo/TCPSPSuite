//
// Created by lukas on 24.10.17.
//

#include "storage.hpp"

#include <odb/sqlite/database.hxx>
#include <odb/transaction.hxx>
#include <odb/schema-catalog.hxx>
#include <odb/exceptions.hxx>
#include <odb/session.hxx>
#include <sstream>

#include "db_objects-odb.hxx"

#include "../util/solverconfig.hpp"

Storage::Storage(std::string filename)
	: db(new odb::sqlite::database(filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE)),
	  l("STORAGE")
{
    BOOST_LOG(l.i()) << "Opening DB: " << filename;
    odb::transaction t (db->begin ());
    try {
        odb::schema_catalog::create_schema(*db, "", false);
        BOOST_LOG(l.i()) << "Created a new database";
    } catch (odb::database_exception & e) {
        BOOST_LOG(l.i()) << "Database already populated";
    }
    t.commit ();
}

long unsigned int
Storage::insert(const Solution & sol, const std::string & run_id, const std::string & algorithm_id, 
                const std::string & config_name, int instance_seed, double elapsed_time, 
                const SolverConfig & sc, const AdditionalResultStorage & additional)
{
    std::lock_guard<std::mutex> guard(this->insert_mutex);

    odb::core::session s;
    odb::core::transaction t (this->db->begin());

    std::shared_ptr<DBConfig> db_sc = this->get_or_insert_solverconfig(sc);

    std::shared_ptr<DBResult> res(new DBResult(
                    run_id, sol.get_instance()->get_id(), sol.get_costs(), algorithm_id, config_name,
                    instance_seed, sol.is_optimal(), sol.is_feasible(), sol.get_lower_bound(),
                    elapsed_time, db_sc));
    auto res_id = db->persist(res.get());
    BOOST_LOG(l.d(3)) << "Stored a result";

    for (const auto & item : additional.intermediate_results) {
        this->insert_intermediate_result(res, item);
    }

    for (const auto & item : additional.extended_measures) {
        this->insert_extended_measure(res, item);
    }

    t.commit();

    return res_id;
}

void
Storage::insert_intermediate_result(std::shared_ptr<DBResult> res,
                                const AdditionalResultStorage::IntermediateResult &intermediate)
{
    std::shared_ptr<DBSolution> solution(nullptr);

    if (intermediate.solution.valid()) {
        solution = this->insert_solution(res, intermediate.solution.value());
    }

    DBIntermediate db_intermediate(
                    res, intermediate.time, intermediate.iteration, intermediate.costs,
                    intermediate.bound, solution);
    this->db->persist(db_intermediate);
}

void
Storage::insert_extended_measure(std::shared_ptr<DBResult> res,
                             const AdditionalResultStorage::ExtendedMeasure &measure)
{
    Maybe<double> v_double;
    Maybe<int> v_int;

    switch(measure.type) {
        case AdditionalResultStorage::ExtendedMeasure::TYPE_DOUBLE:
            v_double = measure.value.v_double;
            break;
        case AdditionalResultStorage::ExtendedMeasure::TYPE_INT:
            v_int = measure.value.v_int;
            break;
        default:
            throw "Unknown measure type!";
    }
    DBExtendedMeasure dbex(
                    res, measure.key, measure.iteration, measure.time, v_int, v_double
    );

    this->db->persist(dbex);
}

std::shared_ptr<DBSolution>
Storage::insert_solution(std::shared_ptr<DBResult> res, const Solution & sol)
{
    std::shared_ptr<DBSolution> db_sol(new DBSolution(res));
    this->db->persist(db_sol.get());

    for (unsigned int jid = 0 ; jid < sol.get_instance()->job_count() ; ++jid) {
        if (sol.job_scheduled(jid)) {
            DBSolutionJob db_job(db_sol, jid, sol.get_start_time(jid));
            this->db->persist(db_job);
        }
    }

    return db_sol;
}

void
Storage::insert_error(const std::string & instance_id, const std::string & run_id,
                      const std::string & algorithm_id, const std::string & config_name, int seed,
                      unsigned int error_id, int fault_code)
{
    std::lock_guard<std::mutex> guard(this->insert_error_mutex);

    odb::core::session s;
    odb::core::transaction t (this->db->begin());

    std::time_t timestamp = std::time(nullptr);

    DBError err((unsigned long)timestamp, run_id, instance_id, algorithm_id, config_name, seed, fault_code,
                (int)error_id);
    this->db->persist(err);

    t.commit();
}

bool
Storage::check_result(const std::string & instance_id, const std::string & run_id,
                      const std::string & algorithm_id, const std::string & config_name,
                      bool only_optimal)
{
    std::lock_guard<std::mutex> guard(this->check_result_mutex);

    odb::core::transaction t (this->db->begin());

    using query = odb::core::query<DBResult>;
    using result = odb::core::result<DBResult>;
    result r;

    if (only_optimal) {
        r = this->db->query<DBResult>(query::instance == instance_id &&
                                      query::run == run_id &&
                                      query::algorithm == algorithm_id &&
                                      query::config == config_name &&
                                      query::optimal == true);
    } else {
        r = this->db->query<DBResult>(query::instance == instance_id &&
                                      query::run == run_id &&
                                      query::algorithm == algorithm_id &&
                                      query::config == config_name );
    }
    result::iterator i(r.begin());
    return i != r.end();
}

std::shared_ptr<DBConfig>
Storage::get_solverconfig(const SolverConfig & sc)
{
    using query = odb::core::query<DBConfig>;
    using result = odb::core::result<DBConfig>;
    result r;

    r = this->db->query<DBConfig>(query::name == query::_val(sc.get_name()));

    for (result::iterator i (r.begin ()); i != r.end (); ++i)
    {
        std::shared_ptr<DBConfig> ret(i.load());
        return ret;
    }

    return std::shared_ptr<DBConfig>(nullptr);
}

template<class T>
std::string
json_to_string(const T & val)
{
    std::stringstream buf;
    buf << val;
    return buf.str();
}

std::shared_ptr<DBConfig>
Storage::get_or_insert_solverconfig(const SolverConfig & sc)
{
    auto db_cfg = this->get_solverconfig(sc);
    if (db_cfg.get() != nullptr) {
        return db_cfg;
    }

    db_cfg.reset(new DBConfig(sc.get_name(), sc.get_time_limit()));
    db->persist(db_cfg.get());

    for (const auto & kv : sc.get_kvs()) {
        std::shared_ptr<DBConfigKV> db_kv(new DBConfigKV(db_cfg, kv.first, json_to_string(kv.second)));
        db->persist(db_kv.get());
    }

    return db_cfg;
}