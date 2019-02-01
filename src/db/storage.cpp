//
// Created by lukas on 24.10.17.
//

#include "storage.hpp"

#include <boost/asio/ip/host_name.hpp>
#include <ctime>
#include <odb/exceptions.hxx>
#include <odb/schema-catalog.hxx>
#include <odb/session.hxx>
#include <odb/sqlite/database.hxx>
#include <odb/transaction.hxx>
#include <sstream>
#include <chrono>

#include "generated_config.hpp"

#include "db_objects-odb-sqlite.hxx"
#include "db_objects-odb.hxx"

#include "../instance/instance.hpp"
#include "../manager/errors.hpp"
#include "../util/fault_codes.hpp"
#include "../util/git.hpp"
#include "../util/solverconfig.hpp"
#include "db_factory.hpp"

template <class T>
std::string
json_to_string(const T & val)
{
	std::stringstream buf;
	buf << val;
	return buf.str();
}

std::vector<unsigned long>
Storage::find_db_configs(const SolverConfig & sc, bool ignore_name)
{
	std::string query_str = "";

	if (sc.get_kvs().empty()) {
		// Make the query happy with something nonsensical
		query_str += "SELECT id FROM DBConfig) AS DummyConfig ON DummyConfig.id = "
		             "DBConfig.id \n";
	}

	if (sc.get_kvs().size() > 0) {
		size_t subqueries = 0;

		bool first = true;
		for (const auto & kv_pair : sc.get_kvs()) {
			std::string sq_name = "SQ" + std::to_string(subqueries++);
			/*
			 * This is a *really*, *really* bad hack.
			 * ODB will automatically insert a "WHERE" into the query if it
			 * doesn't consider our query string a "complete query". "Complete
			 * queries" may start with a set of strings, "SELECT" is one of them.
			 *
			 * Thus, we supply the first "INNER JOIN (" in the static part of the
			 * query string and start our dynamic part with "SELECT".
			 */

			if (!first) {
				query_str += " INNER JOIN (";
			}
			first = false;
			query_str += "SELECT * from DBConfigKV ";
			query_str += " WHERE key = '" + kv_pair.first + "' ";
			query_str += " AND value = '" + json_to_string(kv_pair.second) + "'";
			query_str += ") as " + sq_name;
			query_str += " ON " + sq_name + ".cfg = DBConfig.id \n";
		}
	}
	query_str += "WHERE ";

	if (!ignore_name) {
		query_str += " DBConfig.name = '" + sc.get_name() + "' AND ";
	}

	query_str += " NOT EXISTS ";
	query_str +=
	    " ( SELECT 1 FROM DBConfigKV WHERE DBConfigKV.cfg = DBConfig.id ";

	if (sc.get_kvs().size() > 0) {
		query_str += "AND NOT (\n";
		bool first = true;
		for (const auto & kv_pair : sc.get_kvs()) {
			if (!first) {
				query_str += " OR ";
			}
			first = false;

			query_str += " ( key = '" + kv_pair.first + "' ";
			query_str += " AND value = '" + json_to_string(kv_pair.second) + "')\n";
		}
		query_str += ")";
	}
	query_str += ")\n";

	//	std::cout << "Running query:\n";
	//	std::cout << "SELECT DBConfig.id from DBConfig INNER JOIN (" << query_str
	//<< std::flush;

	auto r = this->db->query<ConfigGetterView>(query_str);

	std::vector<unsigned long> ids;
	auto i = r.begin();
	while (i != r.end()) {
		ids.push_back(i->config_id);
		++i;
	}

	return ids;
}

Storage::Storage(std::string filename, unsigned int retry_count_in)
    : retry_count(retry_count_in), db(nullptr), l("STORAGE")
{
	BOOST_LOG(l.i()) << "Opening DB: " << filename;

	DBFactory dbf;
	this->db = dbf.get(filename);

	odb::transaction t(db->begin());
	try {
		odb::schema_catalog::create_schema(*db, "", false);
		BOOST_LOG(l.i()) << "Created a new database";
	} catch (odb::database_exception & e) {
		BOOST_LOG(l.i()) << "Database already populated";
	}
	t.commit();
}

void
Storage::initialize(std::string filename, int argc, const char ** argv)
{
	std::string cmdline = "";
	for (int i = 0; i < argc; ++i) {
		if (i > 0) {
			cmdline += std::string(" ");
		}
		cmdline += std::string(argv[i]);
	}

	Storage::invocation.reset(
	    new DBInvocation(cmdline, GIT_SHA1, boost::asio::ip::host_name(),
	                     (unsigned long)std::time(nullptr)));

	Storage s(filename);
	odb::transaction t(s.db->begin());
	s.db->persist(Storage::invocation);
	t.commit();
}

std::shared_ptr<DBInvocation>
Storage::get_invocation()
{
	return Storage::invocation;
}

long unsigned int
Storage::insert(const Solution & sol, const std::string & run_id,
                const std::string & algorithm_id,
                const std::string & config_name, int instance_seed,
                double elapsed_time, const SolverConfig & sc,
                const AdditionalResultStorage & additional,
                const manager::LinuxMemoryInfo * mem_info,
                const manager::PAPIPerformanceInfo * papi_info)
{
	std::lock_guard<std::mutex> guard(Storage::insert_mutex);

	for (unsigned int trial = 0; trial < this->retry_count; ++trial) {
		try {
			odb::core::session s;
			odb::core::transaction t(this->db->begin());

			std::shared_ptr<DBConfig> db_sc = this->get_or_insert_solverconfig(sc);

			std::shared_ptr<DBResult> res(new DBResult(
			    run_id, sol.get_instance()->get_id(), sol.get_costs(), algorithm_id,
			    config_name, instance_seed, sol.is_optimal(), sol.is_feasible(),
			    sol.get_lower_bound(), elapsed_time, db_sc,
			    Storage::get_invocation()));
			auto res_id = db->persist(res);
			BOOST_LOG(l.d(3)) << "Stored a result";

			if (mem_info != nullptr) {
#ifdef INSTRUMENT_MALLOC
				odb::nullable<size_t> malloc_count(mem_info->get_malloc_count());
				odb::nullable<size_t> malloc_max_size(mem_info->get_malloc_max_size());
				odb::nullable<size_t> rss_bytes_max;
				odb::nullable<size_t> data_bytes_max;
#else
				odb::nullable<size_t> malloc_count;
				odb::nullable<size_t> malloc_max_size;
				odb::nullable<size_t> rss_bytes_max(mem_info->get_rss_bytes_max());
				odb::nullable<size_t> data_bytes_max(mem_info->get_data_bytes_max());
#endif

				std::shared_ptr<DBResourcesInfo> res_info(new DBResourcesInfo(
				    res, rss_bytes_max, data_bytes_max, malloc_max_size, malloc_count,
				    mem_info->get_major_pagefaults(), mem_info->get_minor_pagefaults(),
				    mem_info->get_user_usecs(), mem_info->get_system_usecs()));
				db->persist(res_info);
				BOOST_LOG(l.d(3)) << "Stored memory measurements data.";
			}

			// The follwing is well-formed only if PAPI was found
#ifdef PAPI_FOUND
			if (papi_info != nullptr) {
				auto papi_results = papi_info->get_counts();

				for (const auto & papi_result : papi_results) {
					std::shared_ptr<DBPapiMeasurement> papi_measure(new DBPapiMeasurement(
					    res, papi_result.first, papi_result.second));
					db->persist(papi_measure);
					BOOST_LOG(l.d(4)) << "Stored PAPI measurement " << papi_result.first;
				}
			}
#else
			(void)papi_info;
#endif

			for (const auto & item : additional.intermediate_results) {
				this->insert_intermediate_result(res, item);
			}

			for (const auto & item : additional.extended_measures) {
				this->insert_extended_measure(res, item);
			}

			t.commit();

			return res_id;
		} catch (odb::recoverable & recoverable) {
			BOOST_LOG(l.w()) << "Database insert() operation failed. Try "
			                 << (trial + 1) << "...";
			BOOST_LOG(l.w()) << "Error message: " << recoverable.what();
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}
	}
	BOOST_LOG(l.e()) << "Too many database failures.";
	throw IOError(*sol.get_instance(), instance_seed, FAULT_DATABASE_FAILED,
	              "Too many database failures");
}

void
Storage::insert_intermediate_result(
    std::shared_ptr<DBResult> res,
    const AdditionalResultStorage::IntermediateResult & intermediate)
{
	std::shared_ptr<DBSolution> solution(nullptr);

	if (intermediate.solution.valid()) {
		solution = this->insert_solution(res, intermediate.solution.value());
	}

	DBIntermediate db_intermediate(res, intermediate.time, intermediate.iteration,
	                               intermediate.costs, intermediate.bound,
	                               solution);
	this->db->persist(db_intermediate);
}

void
Storage::insert_extended_measure(
    std::shared_ptr<DBResult> res,
    const AdditionalResultStorage::ExtendedMeasure & measure)
{
	Maybe<double> v_double;
	Maybe<int> v_int;

	switch (measure.type) {
	case AdditionalResultStorage::ExtendedMeasure::TYPE_DOUBLE:
		v_double = measure.value.v_double;
		break;
	case AdditionalResultStorage::ExtendedMeasure::TYPE_INT:
		v_int = measure.value.v_int;
		break;
	default:
		throw "Unknown measure type!";
	}
	DBExtendedMeasure dbex(res, measure.key, measure.iteration, measure.time,
	                       v_int, v_double);

	this->db->persist(dbex);
}

std::shared_ptr<DBSolution>
Storage::insert_solution(std::shared_ptr<DBResult> res, const Solution & sol)
{
	std::shared_ptr<DBSolution> db_sol(new DBSolution(res));
	this->db->persist(db_sol);

	for (unsigned int jid = 0; jid < sol.get_instance()->job_count(); ++jid) {
		if (sol.job_scheduled(jid)) {
			DBSolutionJob db_job(db_sol, jid, sol.get_start_time(jid));
			this->db->persist(db_job);
		}
	}

	return db_sol;
}

void
Storage::insert_error(const std::string & instance_id,
                      const std::string & run_id,
                      const std::string & algorithm_id,
                      const std::string & config_name, int seed,
                      unsigned int error_id, int fault_code)
{
	std::lock_guard<std::mutex> guard(Storage::insert_error_mutex);

	for (unsigned int trial = 0; trial < this->retry_count; ++trial) {
		try {
			odb::core::session s;
			odb::core::transaction t(this->db->begin());

			std::time_t timestamp = std::time(nullptr);

			DBError err((unsigned long)timestamp, run_id, instance_id, algorithm_id,
			            config_name, seed, fault_code, (int)error_id);
			this->db->persist(err);

			t.commit();
			return;
		} catch (odb::recoverable & recoverable) {
			BOOST_LOG(l.w()) << "Database insert_error() operation failed. Try "
			                 << (trial + 1) << "...";
			BOOST_LOG(l.w()) << "Error message: " << recoverable.what();
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}
	}
	BOOST_LOG(l.e()) << "Too many database failures. Not throwing exception to "
	                    "avoid deadlock. Goodbye.";
	exit(-1);
}

bool
Storage::check_error(std::vector<int> error_ids, std::vector<int> fault_codes,
                     const std::string & instance_id,
                     const std::string & run_id,
                     const std::string & algorithm_id, const SolverConfig & sc,
                     bool ignore_config_name, bool ignore_run_name)
{
	BOOST_LOG(l.d(1)) << "Checking for error";

	// ODB *should* handle locking
	//std::lock_guard<std::mutex> guard(Storage::check_error_mutex);

	// TODO FIXME TMP
	//BOOST_LOG(l.d(1)) << "Got the lock.";
	
	for (unsigned int trial = 0; trial < this->retry_count; ++trial) {
		try {
			odb::transaction t(this->db->begin());
			odb::session s;

			using query = odb::query<DBError>;

			auto q = (query::instance == instance_id) &&
			         (query::algorithm == algorithm_id);
			if (!ignore_config_name) {
				q = q && (query::config == sc.get_name());
			}
			if (!ignore_run_name) {
				q = q && (query::run == run_id);
			}

			if (!error_ids.empty()) {
				q = q && (query::error_id.in_range(error_ids.begin(), error_ids.end()));
			}
			if (!fault_codes.empty()) {
				q = q && (query::fault_code.in_range(fault_codes.begin(),
				                                     fault_codes.end()));
			}

			// auto r = this->db->query<DBError>(q);
			auto r = this->db->query_one<DBError>(q);

			// TODO FIXME check actual solverconfig parameter values

			return r.get() != nullptr;
		} catch (odb::recoverable & recoverable) {
			BOOST_LOG(l.w()) << "Database check_error() operation failed. Try "
			                 << (trial + 1) << "...";
			BOOST_LOG(l.w()) << "Error message: " << recoverable.what();
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}
	}
	BOOST_LOG(l.e()) << "Too many database failures. Assuming no error.";
	return false;
	// return (r.begin() != r.end());
}

bool
Storage::check_result(const std::string & instance_id,
                      const std::string & run_id,
                      const std::string & algorithm_id, const SolverConfig & sc,
                      bool only_optimal, bool ignore_config_name,
                      bool ignore_run_name)
{
	std::lock_guard<std::mutex> guard(Storage::check_result_mutex);

	for (unsigned int trial = 0; trial < this->retry_count; ++trial) {
		try {
			odb::transaction t(this->db->begin());
			odb::session s;

			using result = odb::core::result<DBResult>;
			result r;

			std::string query_str = "SELECT * FROM DBResult \n";

			size_t subqueries = 0;

			for (const auto & kv_pair : sc.get_kvs()) {
				std::string sq_name = "SQ" + std::to_string(subqueries++);
				query_str += " INNER JOIN (SELECT * from DBConfigKV ";
				query_str += " WHERE key = '" + kv_pair.first + "' ";
				query_str += " AND value = '" + json_to_string(kv_pair.second) + "'";
				query_str += ") as " + sq_name;
				query_str += " ON " + sq_name + ".cfg = DBResult.cfg \n";
			}

			query_str += "    WHERE \n";
			query_str += " DBResult.instance = '" + instance_id + "' ";
			query_str += " AND DBResult.algorithm = '" + algorithm_id + "' ";
			if (!ignore_config_name) {
				query_str += " AND DBResult.config = '" + sc.get_name() + "' ";
			}
			if (!ignore_run_name) {
				query_str += " AND DBResult.run = '" + run_id + "' ";
			}
			if (only_optimal) {
				query_str += " AND optimal > 0";
			}

			query_str += "\n    AND NOT EXISTS ";
			query_str += " ( SELECT 1 FROM DBConfigKV WHERE DBConfigKV.cfg = "
			             "DBResult.cfg ";

			if (sc.get_kvs().size() > 0) {
				query_str += "AND NOT (\n";
				bool first = true;
				for (const auto & kv_pair : sc.get_kvs()) {
					if (!first) {
						query_str += " OR ";
					}
					first = false;

					query_str += " ( key = '" + kv_pair.first + "' ";
					query_str +=
					    " AND value = '" + json_to_string(kv_pair.second) + "')\n";
				}
				query_str += ")";
			}
			query_str += ");";

			BOOST_LOG(l.d(5)) << "Executing query:";
			BOOST_LOG(l.d(5)) << query_str;
			//	std::cout << query_str << std::flush;

			size_t rows = this->db->execute(query_str);
			return (rows > 0);
		} catch (odb::recoverable & recoverable) {
			BOOST_LOG(l.w()) << "Database check_result() operation failed. Try "
			                 << (trial + 1) << "...";
			BOOST_LOG(l.w()) << "Error message: " << recoverable.what();
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}
	}
	BOOST_LOG(l.e()) << "Too many database failures. Assuming no result.";
	return false;
}

std::shared_ptr<DBConfig>
Storage::find_equivalent_config(std::shared_ptr<DBConfig> src)
{
	// TODO FIXME shouldn't this by guarded?

	for (unsigned int trial = 0; trial < this->retry_count; ++trial) {
		try {

			using query = odb::core::query<DBConfig>;
			using result = odb::core::result<DBConfig>;
			result r;

			r = this->db->query<DBConfig>(query::name == query::_val(src->name));

			result::iterator i(r.begin());
			while (i != r.end()) {
				std::shared_ptr<DBConfig> ret(i.load());

				std::map<std::string, std::string> stored_kvs;

				for (auto kv : ret->entries) {
					stored_kvs[kv->key] = kv->value;
				}

				bool matches = true;
				size_t seen_kvs = 0;

				for (const auto & kv : src->entries) {
					if ((stored_kvs.find(kv->key) == stored_kvs.end()) ||
					    (stored_kvs.at(kv->key) != json_to_string(kv->value))) {
						matches = false;
						break;
					}
					seen_kvs++;
				}

				matches &= (seen_kvs == stored_kvs.size());

				if (matches) {
					return ret;
				}

				i++;
			}

			return std::shared_ptr<DBConfig>(nullptr);
		} catch (odb::recoverable & recoverable) {
			BOOST_LOG(l.w())
			    << "Database find_equivalent_config() operation failed. Try "
			    << (trial + 1) << "...";
			BOOST_LOG(l.w()) << "Error message: " << recoverable.what();
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}
	}
	BOOST_LOG(l.e()) << "Too many database failures. Assuming no config.";
	return std::shared_ptr<DBConfig>(nullptr);
}

std::shared_ptr<DBConfig>
Storage::get_solverconfig(const SolverConfig & sc)
{
	std::vector<unsigned long> dbcfg_ids = this->find_db_configs(sc);

	if (dbcfg_ids.empty()) {
		return nullptr;
	}

	// TODO FIXME After merging, this seems to be broken?
	//assert(dbcfg_ids.size() == 1);
	
	using query = odb::query<DBConfig>;
	auto r = this->db->query<DBConfig>(query::id == query::_val(dbcfg_ids[0]));

	auto i = r.begin();
	std::shared_ptr<DBConfig> ret(i.load());
	return ret;
}

std::vector<std::shared_ptr<DBResult>>
Storage::get_results_for_config(const SolverConfig & sc)
{
	// TODO shouldn't this be guarded?
	for (unsigned int trial = 0; trial < this->retry_count; ++trial) {
		try {

			odb::transaction t(this->db->begin());
			odb::session s;
			auto scfg_ids = this->find_db_configs(sc, true);
			std::vector<std::shared_ptr<DBResult>> ret;

			for (unsigned long id : scfg_ids) {
				using query = odb::query<DBResult>;
				auto r = this->db->query<DBResult>(query::cfg == id);

				auto i = r.begin();
				while (i != r.end()) {
					ret.push_back(i.load());
					++i;
				}
			}

			return ret;
		} catch (odb::recoverable & recoverable) {
			BOOST_LOG(l.w()) << "Database get_results_for_config() operation failed. Try "
			                 << (trial + 1) << "...";
			BOOST_LOG(l.w()) << "Error message: " << recoverable.what();
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}
	}
	BOOST_LOG(l.e()) << "Too many database failures. Assuming no result.";
	return {};
}

std::shared_ptr<DBConfig>
Storage::get_or_insert_solverconfig(const SolverConfig & sc)
{
	auto db_cfg = this->get_solverconfig(sc);
	if (db_cfg.get() != nullptr) {
		return db_cfg;
	}

	db_cfg.reset(new DBConfig(sc.get_name(), sc.get_time_limit()));
	db->persist(db_cfg);

	for (const auto & kv : sc.get_kvs()) {
		std::shared_ptr<DBConfigKV> db_kv(
		    new DBConfigKV(db_cfg, kv.first, json_to_string(kv.second)));
		db->persist(db_kv);
	}

	return db_cfg;
}

std::mutex Storage::insert_mutex;
std::mutex Storage::insert_error_mutex;
std::mutex Storage::check_result_mutex;
std::mutex Storage::check_error_mutex;
std::shared_ptr<DBInvocation> Storage::invocation;
