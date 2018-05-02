#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <string>                       // for string, allocator
#include <vector>                       // for vector
#include "../datastructures/maybe.hpp"  // for Maybe
#include "../util/log.hpp"              // for Log
#include "solverconfig.hpp"             // for SolverConfig

using json = nlohmann::json;

class Configuration {
public:
  static Configuration * get() {
	  if (instance == nullptr) {
		  instance = new Configuration;
	  }
    return instance;
  }

	bool parse_cmdline(int argc, const char **argv);

  void set_solver_config(SolverConfig sc) {
    this->solver_cfgs.clear();
    this->solver_cfgs.push_back(sc);
  }
  
  void set_solver_configs(std::vector<SolverConfig> scs) {
    this->solver_cfgs = scs;
  }

  void read_solver_config(std::string filename) {
    this->solver_cfgs = SolverConfig::read_configs(filename);
  }

  const std::vector<SolverConfig> & solver_configs() const
  {
    return this->solver_cfgs;
  }

	void 
	set_storage_path(std::string path);
  
	const std::string &
	get_storage_path() const;
	
	void 
	set_instance_dir(Maybe<std::string> path);

	const Maybe<std::string> &
	get_instance_dir() const;
	
	void 
	set_instance_file(Maybe<std::string> path);

	const Maybe<std::string> &
	get_instance_file() const;
	
	void 
	set_algo_config_file(Maybe<std::string> path);

	const Maybe<std::string> &
	get_algo_config_file() const;
	
	void 
	set_algo_regexp(Maybe<std::string> regex);

	const Maybe<std::string> &
	get_algo_regexp() const;
	
	void 
	set_time_limit(Maybe<unsigned int> seconds);

	const Maybe<unsigned int> &
	get_time_limit() const;
	
	void 
	set_run(std::string id);

	const std::string &
	get_run() const;
	
	void 
	set_instance_seed(Maybe<int> seed);

	const Maybe<int> &
	get_instance_seed() const;
	
	void 
	set_global_seed(Maybe<int> seed);

	const Maybe<int> &
	get_global_seed() const;
	
	void 
	set_log_dir(Maybe<std::string> path);

	const Maybe<std::string> &
	get_log_dir() const;
	
	void 
	set_result_dir(Maybe<std::string> path);

	const Maybe<std::string> &
	get_result_dir() const;
	
	void 
	set_skip_done(bool skip);

	bool
	get_skip_done() const;
	
	void 
	set_threads(Maybe<unsigned int> t);

	Maybe<unsigned int>
	get_threads() const;
	
	void 
	set_parallelism(unsigned int p);

	unsigned int
	get_parallelism() const;

	Configuration(const Configuration &) = delete;

private:
  Configuration();
	void set_defaults();

	static Configuration * instance;

	// actual options
	std::string           storage_path;
	Maybe<std::string>    instance_dir;
	Maybe<std::string>    instance_file;
	Maybe<std::string>    algo_config_file;
	Maybe<std::string>    algo_regexp;
	Maybe<unsigned int>   time_limit;
	Maybe<unsigned int>   threads;
	unsigned int          parallelism;
	std::string           run;
	bool                  skip_done;
	Maybe<int>            instance_seed;
	Maybe<int>            global_seed;
	Maybe<std::string>    log_dir;
	Maybe<std::string>    result_dir;

	std::vector<SolverConfig> solver_cfgs;

	Log l;
};

#endif
