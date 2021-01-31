#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include "../datastructures/maybe.hpp" // for Maybe
#include "../util/filetools.hpp"
#include "../util/log.hpp"  // for Log
#include "solverconfig.hpp" // for SolverConfig

#include <string> // for string, allocator
#include <vector> // for vector

using json = nlohmann::json;

class Configuration {
public:
	static Configuration *
	get()
	{
		if (instance == nullptr) {
			instance = new Configuration;
		}
		return instance;
	}

	bool parse_cmdline(int argc, const char ** argv);

	void
	set_solver_config(SolverConfig sc)
	{
		this->solver_cfgs.clear();
		this->solver_cfgs.push_back(sc);
	}

	void
	set_solver_configs(std::vector<SolverConfig> scs)
	{
		this->solver_cfgs = scs;
	}

	void
	read_solver_config(std::string filename)
	{
		try {
			this->solver_cfgs = SolverConfig::read_configs(filename);
		} catch (json::parse_error e) {
			BOOST_LOG(l.e()) << "JSON Parsing error in solver configuration.";
			BOOST_LOG(l.e()) << e.what();
			BOOST_LOG(l.e()) << "Error is near: ";

			util::FileContextGiver fcg(filename, e.byte);

			for (const auto & line : fcg.get_message()) {
				BOOST_LOG(l.e()) << line;
			}

			throw std::move(e);
		}
	}

	const std::vector<SolverConfig> &
	solver_configs() const
	{
		return this->solver_cfgs;
	}

	void set_storage_path(std::string path);

	const std::string & get_storage_path() const;

	void set_instance_dir(Maybe<std::string> path);

	const Maybe<std::string> & get_instance_dir() const;

	void set_instance_file(Maybe<std::string> path);

	const Maybe<std::string> & get_instance_file() const;

	void set_algo_config_file(Maybe<std::string> path);

	const Maybe<std::string> & get_algo_config_file() const;

	void set_algo_regexp(Maybe<std::string> regex);

	const Maybe<std::string> & get_algo_regexp() const;

	void set_time_limit(Maybe<unsigned int> seconds);

	const Maybe<unsigned int> & get_time_limit() const;

	void set_run(std::string id);

	const std::string & get_run() const;

	void set_instance_seed(Maybe<int> seed);

	const Maybe<int> & get_instance_seed() const;

	void set_global_seed(Maybe<int> seed);

	const Maybe<int> & get_global_seed() const;

	void set_log_dir(Maybe<std::string> path);

	const Maybe<std::string> & get_log_dir() const;

	void set_result_dir(Maybe<std::string> path);

	const Maybe<std::string> & get_result_dir() const;

	void set_skip_done(bool skip);

	bool get_skip_done() const;

	void set_threads(Maybe<unsigned int> t);

	Maybe<unsigned int> get_threads() const;

	bool are_memory_metrics_enabled() const;

	unsigned int get_meminfo_sampling_time() const;

	const std::vector<std::string> & get_papi_metrics() const;

	void set_parallelism(unsigned int p);

	unsigned int get_parallelism() const;

	void set_partition_count(unsigned int pc);

	Maybe<unsigned int> get_partition_count() const;

	void set_partition_number(unsigned int pn);
	Maybe<unsigned int> get_partition_number() const;

	void set_skip_oom(bool skip);
	bool get_skip_oom() const;

	void set_thread_check_time(Maybe<double> seconds);
	Maybe<double> get_thread_check_time() const;

	Configuration(const Configuration &) = delete;

private:
	Configuration();
	void set_defaults();

	static Configuration * instance;

	// actual options
	std::string storage_path;
	Maybe<std::string> instance_dir;
	Maybe<std::string> instance_file;
	Maybe<std::string> algo_config_file;
	Maybe<std::string> algo_regexp;
	Maybe<unsigned int> time_limit;
	Maybe<unsigned int> threads;
	bool enable_memory_metrics;
	unsigned int meminfo_sampling_time;
	std::vector<std::string> papi_metrics;
	unsigned int parallelism;
	std::string run;
	bool skip_done;
	Maybe<int> instance_seed;
	Maybe<int> global_seed;
	Maybe<std::string> log_dir;
	Maybe<std::string> result_dir;
	Maybe<unsigned int> partition_count;
	Maybe<unsigned int> partition_number;
	bool skip_oom;
	Maybe<double> thread_check_time;

	std::vector<SolverConfig> solver_cfgs;

	Log l;
};

#endif
