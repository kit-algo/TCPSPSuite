#include "dbmerger.hpp"
#include "db/storage.hpp"                       // for Storage
#include "util/log.hpp"                         // for Log
#include <algorithm>                            // for move
#include <assert.h>                             // for assert
#include <boost/log/core/record.hpp>            // for record
#include <boost/log/sources/record_ostream.hpp> // for operator<<, basic_re...
#include <db_objects.hpp>                       // for DBResult, DBIntermed...
#include <iostream>                             // for operator<<, cout
#include <memory>                               // for __shared_ptr_access
#include <odb/database.hxx>                     // for database
#include <odb/exceptions.hxx>
#include <odb/schema-catalog.hxx>
#include <odb/session.hxx>
#include <odb/sqlite/database.hxx>
#include <odb/transaction.hxx>
#include <set> // for set, _Rb_tree_const_...
#include <sstream>
#include <stddef.h> // for size_t

#include "generated_config.hpp"

#include "../db/db_objects-odb-sqlite.hxx"
#include "../db/db_objects-odb.hxx"

DBMerger::DBMerger(std::string dest_filename)
    : dest(dest_filename), l("DBMERGE")
{
	BOOST_LOG(l.i()) << "Destination DB: " << dest_filename;
}

void
DBMerger::merge(std::string src_filename)
{
	Storage src(src_filename);

	BOOST_LOG(l.i()) << " === Merging from " << src_filename;

	this->copy_from(src);
}

std::shared_ptr<DBConfig>
DBMerger::copy_config(std::shared_ptr<DBConfig> src)
{
	std::shared_ptr<DBConfig> res = this->dest.find_equivalent_config(src);
	if (res.get() != nullptr) {
		return res;
	}

	res.reset(new DBConfig(src));
	this->dest.db->persist(res);

	// Copy over the key-value pairs
	for (const auto & src_kv : src->entries) {
		std::shared_ptr<DBConfigKV> kv_copy{new DBConfigKV(src_kv)};
		kv_copy->cfg = res;
		this->dest.db->persist(kv_copy);
	}

	return res;
}

std::shared_ptr<DBResult>
DBMerger::copy_result(std::shared_ptr<DBResult> src)
{
	auto invocation_id = src->invocation->get_id();
	std::shared_ptr<DBInvocation> invocation;
	if (this->invocations.find(invocation_id) == this->invocations.end()) {
		std::shared_ptr<DBInvocation> iv_cpy{new DBInvocation(src->invocation)};
		this->dest.db->persist(iv_cpy);
		this->invocations.insert({invocation_id, iv_cpy});
		invocation = iv_cpy;
	} else {
		invocation = this->invocations.at(invocation_id);
	}

	std::shared_ptr<DBResult> res{new DBResult(src)};
	auto cfg = this->copy_config(src->cfg);

	res->cfg = cfg;
	res->invocation = invocation;
	this->dest.db->persist(res);

	return res;
}

std::shared_ptr<DBResourcesInfo>
DBMerger::copy_resinfo(std::shared_ptr<DBResourcesInfo> src,
                       std::shared_ptr<DBResult> result)
{
	std::shared_ptr<DBResourcesInfo> cpy{new DBResourcesInfo(src)};
	cpy->res = result;
	this->dest.db->persist(cpy);

	return cpy;
}

std::shared_ptr<DBPapiMeasurement>
DBMerger::copy_papi_measurement(std::shared_ptr<DBPapiMeasurement> src,
                                std::shared_ptr<DBResult> result)
{
	std::shared_ptr<DBPapiMeasurement> cpy{new DBPapiMeasurement(src)};
	cpy->res = result;
	this->dest.db->persist(cpy);

	return cpy;
}

std::shared_ptr<DBSolution>
DBMerger::copy_solution(std::shared_ptr<DBSolution> src,
                        std::shared_ptr<DBResult> result)
{
	std::shared_ptr<DBSolution> cpy{new DBSolution(src)};
	cpy->res = result;
	this->dest.db->persist(cpy);

	for (auto job : src->jobs) {
		std::shared_ptr<DBSolutionJob> job_cpy{new DBSolutionJob(job)};
		job_cpy->sol = cpy;
		this->dest.db->persist(job_cpy);
	}

	return cpy;
}

std::shared_ptr<DBIntermediate>
DBMerger::copy_intermediate(std::shared_ptr<DBIntermediate> src,
                            std::shared_ptr<DBResult> result,
                            std::shared_ptr<DBSolution> solution)
{
	std::shared_ptr<DBIntermediate> cpy{new DBIntermediate(src)};
	cpy->res = result;
	if (solution.get() != nullptr) {
		cpy->solution = solution;
	}
	this->dest.db->persist(cpy);

	return cpy;
}

std::shared_ptr<DBError>
DBMerger::copy_error(std::shared_ptr<DBError> src)
{
	std::shared_ptr<DBError> cpy{new DBError(src)};
	odb::transaction t(this->dest.db->begin());
	this->dest.db->persist(cpy);
	t.commit();

	return cpy;
}

std::shared_ptr<DBExtendedMeasure>
DBMerger::copy_extended_measure(std::shared_ptr<DBExtendedMeasure> src,
                                std::shared_ptr<DBResult> result)
{
	std::shared_ptr<DBExtendedMeasure> cpy{new DBExtendedMeasure(src)};
	cpy->res = result;
	this->dest.db->persist(cpy);

	return cpy;
}

void
DBMerger::copy_from(Storage & src)
{
	this->invocations.clear();

	odb::session s;
	const size_t CHUNK_SIZE = 500;

	// Chunked loading
	size_t first_id = 0;
	bool done = false;
	while (!done) {

		odb::transaction t(src.db->begin());
		using query = odb::query<DBResult>;
		odb::result<DBResult> r = src.db->query<DBResult>(
		    ((query::id >= first_id) + " ORDER BY " + query::id) + (" LIMIT 500"),
		    true);

		size_t count = 0;

		decltype(r)::iterator i(r.begin());

		BOOST_LOG(l.i()) << "Loading results...";

		std::vector<std::shared_ptr<DBResult>> queue;

		while (i != r.end()) {
			std::shared_ptr<DBResult> src_res(i.load());
			first_id = std::max(first_id, (src_res->get_id() + 1));
			queue.push_back(src_res);
			i++;
			count++;
		}

		t.commit();

		if (count < CHUNK_SIZE) {
			done = true;
		}

		BOOST_LOG(l.i()) << "Got " << queue.size() << " results.";

		odb::transaction t_dest(this->dest.db->begin());
		while (!queue.empty()) {
			std::shared_ptr<DBResult> src_res = queue.back();
			queue.pop_back();

			std::shared_ptr<DBResult> cpy_res = this->copy_result(src_res);

			for (auto src_obj : src_res->resource_infos) {
				this->copy_resinfo(src_obj, cpy_res);
			}
			for (auto src_obj : src_res->papi_measurements) {
				this->copy_papi_measurement(src_obj, cpy_res);
			}
			std::set<size_t> intermediate_done;
			for (std::shared_ptr<DBSolution> src_obj : src_res->solutions) {
				auto sol_cpy = this->copy_solution(src_obj, cpy_res);
				for (auto src_intermediate : src_obj->intermediates) {
					auto int_cpy =
					    this->copy_intermediate(src_intermediate, cpy_res, sol_cpy);
					intermediate_done.insert(src_intermediate->get_id());
				}
			}
			for (std::shared_ptr<DBIntermediate> src_obj : src_res->intermediates) {
				if (intermediate_done.find(src_obj->get_id()) ==
				    intermediate_done.end()) {
					this->copy_intermediate(src_obj, cpy_res, nullptr);
				}
			}
			for (auto src_obj : src_res->extended_measures) {
				this->copy_extended_measure(src_obj, cpy_res);
			}
		}
		t_dest.commit();
	}

	// Copy errors
	odb::transaction t2{src.db->begin()};
	auto error_r = src.db->query<DBError>();
	auto error_i = error_r.begin();

	std::vector<std::shared_ptr<DBError>> equeue;
	while (error_i != error_r.end()) {
		std::shared_ptr<DBError> src_err(error_i.load());
		equeue.push_back(src_err);
		error_i++;
	}
	t2.commit();

	odb::transaction t3{this->dest.db->begin()};
	while (!equeue.empty()) {
		std::shared_ptr<DBError> src_err = equeue.back();
		equeue.pop_back();
		std::shared_ptr<DBError> cpy_err{new DBError(src_err)};
		this->dest.db->persist(cpy_err);
	}
	t3.commit();
}

int
main(int argc, char ** argv)
{
	std::cout << "======================================\n";
	std::cout << "===   TCPSPSuite Database Merger   ===\n";
	std::cout << "======================================\n";

	assert(argc >= 3);

	DBMerger merger{argv[1]};

	for (unsigned int i = 2; i < argc; ++i) {
		merger.merge(argv[i]);
	}
}
