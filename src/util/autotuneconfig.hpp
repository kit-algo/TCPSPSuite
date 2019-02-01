#ifndef AUTOTUNECONFIG_H
#define AUTOTUNECONFIG_H

#include "../datastructures/maybe.hpp" // for Maybe
#include "../util/log.hpp"             // for Log
#include "parameter.hpp"               // for Parameters
#include "solverconfig.hpp"            // for SolverConfig
#include <json.hpp>                    // for json
#include <string>                      // for string, allocator
#include <vector>                      // for vector

using json = nlohmann::json;

class AutotuneConfig {
public:
	static AutotuneConfig *
	get()
	{
		if (instance == nullptr) {
			instance = new AutotuneConfig;
		}
		return instance;
	}

	bool parse_cmdline(int argc, const char ** argv);

	/**
	 * sets the state of Configuration for the next run
	 */
	bool nextConfig();

	/**
	 * @return the current configuration
	 */
	SolverConfig generateConfig();

	AutotuneConfig(const AutotuneConfig &) = delete;

private:
	AutotuneConfig();
	int current_config;
	size_t current_parameter_group;

	void read_auto_config(std::string file);

	static AutotuneConfig * instance;

	std::vector<std::vector<Parameter>> parameters;
	Log l;
};

#endif
