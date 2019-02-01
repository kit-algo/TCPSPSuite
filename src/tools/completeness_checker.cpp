#include "completeness_checker.hpp"

#include "../contrib/tinydir.h" // for tinydir_next

#include "../db/storage.hpp"
#include "../instance/instance.hpp"
#include "../io/jsonreader.hpp"
#include "../util/autotuneconfig.hpp"

#include "../db/db_objects-odb-sqlite.hxx"
#include "../db/db_objects-odb.hxx"

/*
 * Headers to get solvers
 */
#include "../manager/solvers.hpp"
#include "solver_headers.hpp"

#include "../baselines/earlyscheduler.hpp"
#include "../util/randomizer.hpp"
#include "../util/solverconfig.hpp"

#include <boost/hana.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/core/to.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>
namespace hana = boost::hana;

CompletenessChecker::CompletenessChecker(bool autotune_mode_in)
    : autotune_mode(autotune_mode_in), l("CCHECKER")
{}

void
CompletenessChecker::run(int argc, const char ** argv)
{
	if (!this->autotune_mode) {
		Configuration & cfg = *Configuration::get();
		cfg.parse_cmdline(argc, argv);

		this->storage_path = cfg.get_storage_path();
		this->config_file = cfg.get_algo_config_file();
		this->instance_dir = cfg.get_instance_dir();

		this->generate_scfgs_tcpspsuite();
	} else {
		AutotuneConfig & acfg = *AutotuneConfig::get();
		acfg.parse_cmdline(argc, argv);
		Configuration & cfg = *Configuration::get();

		this->storage_path = cfg.get_storage_path();
		this->instance_dir = cfg.get_instance_dir();

		this->generate_scfgs_autotune();
	}
	this->read_instances();

	this->check();
}

void
CompletenessChecker::generate_scfgs_tcpspsuite()
{
	Configuration & cfg = *Configuration::get();
	cfg.read_solver_config(this->config_file);
	this->scfgs = cfg.solver_configs();
}

void
CompletenessChecker::generate_scfgs_autotune()
{
	AutotuneConfig & acfg = *AutotuneConfig::get();
	do {
		this->scfgs.push_back(acfg.generateConfig());
	} while (acfg.nextConfig());
}

void
CompletenessChecker::check()
{
	Storage storage(this->storage_path);

	/*
	 * Read all the instances
	 */
	std::vector<std::pair<std::shared_ptr<Instance>, std::string>>
	    parsed_instances;
	for (std::string instance_fname : this->instances) {
		JsonReader reader(instance_fname);
		std::shared_ptr<Instance> instance{reader.parse()};
		parsed_instances.push_back({instance, instance_fname});
	}

	for (const SolverConfig & scfg : this->scfgs) {
		/*
		 * See what we have for this solver config
		 */
		using ResultId = std::pair<std::string, std::string>;
		std::set<ResultId> computed_results;
		auto results_for_scfg = storage.get_results_for_config(scfg);
		for (auto result : results_for_scfg) {
			computed_results.insert({result->instance, result->algorithm});
		}

		for (auto & instance_pair : parsed_instances) {
			std::shared_ptr<Instance> instance = instance_pair.first;

			constexpr unsigned int max_N = solvers::get_free_N<void>() - 1;
			// get the set of registered solvers
			auto registered_solvers = solvers::registry_hook<max_N>{}();

			hana::for_each(registered_solvers, [&](auto solver_cls) {
				if (scfg.match(decltype(solver_cls)::type::get_id())) {
					// Found a combination of solver, solver-config and instance!
					ResultId required_result = {instance->get_id(),
					                            decltype(solver_cls)::type::get_id()};
					if (computed_results.find(required_result) ==
					    computed_results.end()) {
						BOOST_LOG(l.w()) << "--- Found a missing result:";
						BOOST_LOG(l.w()) << "       Instance ID:  " << instance->get_id();
						BOOST_LOG(l.w()) << "       Algorithm:    "
						                 << decltype(solver_cls)::type::get_id();
						BOOST_LOG(l.w()) << "       Config:    ";
						for (const auto & cfg_kv : scfg.get_kvs()) {
							std::stringstream buf;
							buf << cfg_kv.second;
							BOOST_LOG(l.w())
							    << "            " << cfg_kv.first << "\t: " << buf.str();
						}
					}
				}
			});
		}
	}
}

void
CompletenessChecker::read_instances()
{
	tinydir_dir dir;
	std::vector<std::string> directories;
	directories.push_back(this->instance_dir);

	while (!directories.empty()) {
		std::string directory = directories.back();
		directories.pop_back();

		tinydir_open(&dir, directory.c_str());

		while (dir.has_next) {
			tinydir_file file;
			tinydir_readfile(&dir, &file);

			std::string filename(file.path);

			if (filename.substr(filename.size() - 1, 1) == ".") {
				tinydir_next(&dir);
				continue;
			}

			if (file.is_dir) {
				directories.push_back(filename);
			} else {
				std::string extension = filename.substr(filename.size() - 5, 5);
				if (extension.compare(".json") == 0) {
					this->instances.push_back(filename);
					BOOST_LOG(l.i()) << "Instance found: " << filename;
				}
			}

			tinydir_next(&dir);
		}
	}
}

int
main(int argc, const char ** argv)
{
	std::vector<std::string> remaining_args;

	enum class MODE
	{
		UNDECIDED,
		AUTOTUNE,
		TCPSPSUITE
	};

	MODE m{MODE::UNDECIDED};

	for (int i = 0; i < argc; ++i) {
		std::string arg = argv[i];

		if (arg == "tcpspsuite") {
			m = MODE::TCPSPSUITE;
		} else if (arg == "autotune") {
			m = MODE::AUTOTUNE;
		} else {
			remaining_args.push_back(arg);
		}
	}

	const char ** remaining_argv =
	    (const char **)malloc(sizeof(const char *) * remaining_args.size());

	for (unsigned int i = 0; i < remaining_args.size(); ++i) {
		remaining_argv[i] = remaining_args[i].c_str();
	}

	bool autotune_mode = (m == MODE::AUTOTUNE);
	CompletenessChecker checker(autotune_mode);
	checker.run((int)remaining_args.size(), remaining_argv);

	return 0;
}
