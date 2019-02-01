#include "autotuneconfig.hpp"
#include "../datastructures/maybe.hpp" // for Maybe
#include "configuration.hpp"
#include "log.hpp"                                 // for Log
#include "solverconfig.hpp"                        // for SolverConfig
#include <boost/lexical_cast/bad_lexical_cast.hpp> // for bad_lexic...
#include <boost/program_options.hpp>
#include <fstream>  // for operator<<
#include <iostream> // for cout

namespace po = boost::program_options;

AutotuneConfig::AutotuneConfig() : current_config(0), l("CONFIG") {}

bool
AutotuneConfig::parse_cmdline(int argc, const char ** argv)
{
	po::options_description desc("TCPSPSuite Options");
	// clang-format off
    desc.add_options()
        ("help,?", "show a help message")
        ("storage,s", po::value<std::string>(), "specifies the path to a sqlite3 database "
                "which will store the results of the computations. The database will be created"
                "if it does not exist.")
        ("instance-dir,d", po::value<std::string>(), "Specifies the path to a directory containing JSON"
                "instance files. The directory will be recursively scanned for JSON files. All instances "
                "found will be computed on one after the other. May not be specified if the -f option"
                " (see below) is used.")
        ("instance-file,f", po::value<std::string>(), "Specifies the path to a single JSON instance "
                "file. Only this instance will be solved. May not be used if -d is used.")
        ("auto-config,c", po::value<std::string>(), "Specifies the path to a single JSON autotune "
                    "configuration file. All combinations of parameters for all algorithms "
                    "specified in the config file will be executed on all instances with the specified "
                    "configurations.")
        ("parallel,p", po::value<unsigned int>(), "Sets the number of instances that should be solved "
                "in parallel. This is independent of the actual solver, it will create separate solver"
                " instances. Note that depending on the actual solver, each solver might spawn "
                "multiple threads, too. See the threads option. Defaults to 1.")
        ("time-limit,l", po::value<unsigned int>(), "Limits the maximum computation time to "
                "<num> seconds. After that time, all solvers will be asked to quit. If -c is "
                "used, the per-algorithm time limit supersedes this option.")
        ("threads,t", po::value<unsigned int>(), "Sets the number of threads that solvers should "
                "use, if they can use multithreading. Defaults to 1. If -c is used, the per-algortihm"
                " thread option supersedes this option. Note that when using the parallel option, "
                "multiple instances of the solver will be run in parallel, thus the number of threads"
                " multiplies!")
        ("run-id,r", po::value<std::string>(), "Sets the run ID, which may be any string. This "
                "string can then later be used to associate the results with a specific run.")
        ("unsolved-only,u", "Asks the software to compute only results which are not yet in the "
                "database. An instance / solver combination is skipped if this combination was "
                "already computed for the specified run (see -r).")
        ("instance-seed,i", po::value<int>(), "Forces the 'instance seed' to be <seed>. THIS IS "
                "PROBABLY NOT WHAT YOU WANT! You most probably want to set a global seed (see -g) "
                "Note that if you use this in combination with -d (see above), all found instances "
                "will be computed with the exact same seed! This is useful for reproducing specific "
                "results. If -c is used, the per-algorithm instance seed may supersede this option.")
        ("seed,g", po::value<int>(), "Forces the 'global seed' to be <seed>. All instance seeds are "
                "generated from this global seed. This is useful to deterministically reproduce a "
                "whole run of the suite.")
        ("log-dir,o", po::value<std::string>(), "Sets <logdir> as path to the log directory. If this "
                "is given, the suite will write a separate log for every solver run into this "
                "directory.")
        ("result-dir,x", po::value<std::string>(), "Sets <resdir> as path to the result dump "
                "directory. If this is given, the suite will write a separate JSON file for every "
                "algorithm run on every instance, dumping the result in JSON format.")
    ("partition-count", po::value<unsigned int>(), "For partitioned computing: Set the "
     		    "number of partitions. This must be set in conjunction with --partition-number. If both "
     		    " are set, the computational tasks are split into <partition-count> partitions of equal "
     		    "size, and this process will compute only one of the partitions.")
    ("partition-number", po::value<unsigned int>(), "For partitioned computing: Set the "
     		    "partition number that this process computes. The first partition is 0, "
     		    "the last partition is <partition-count> - 1. Make sure that every process "
     		    "computes a unique partition!")
	  ("skip-oom", "Similar to -u/--unsolved-only: Causes TCPSPSuite to skip computation of "
	          "any results for which computation has already been tried and resulted in an "
	          "out-of-memory situation. WARNING: Currently, config-comparison is done only based "
	          "on the name of the configurations. Make sure that all algorithm configs have a distinct name!")
	  ("thread-check-time", po::value<double>(), "Setting this to <seconds> causes TCPSPSuite to periodically"
	          "check whether all threads are still alive. A thread is considered to be alive if it completed"
	          " a result within the last <seconds> seconds. This is mainly useful for debugging purposes.")
	    ;
	// clang-format on

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	if (vm.count("help")) {
		std::cout << desc << "\n";
		return false;
	}

	Configuration & cfg = *Configuration::get();

	if (vm.count("storage")) {
		cfg.set_storage_path(vm["storage"].as<std::string>());
	} else {
		BOOST_LOG(l.e()) << "You have to specify a storage path.\n";
		return false;
	}

	if (vm.count("instance-dir")) {
		cfg.set_instance_dir(vm["instance-dir"].as<std::string>());
	}
	if (vm.count("instance-file")) {
		cfg.set_instance_file(vm["instance-file"].as<std::string>());
	}
	if (cfg.get_instance_dir().valid() == cfg.get_instance_file().valid()) {
		BOOST_LOG(l.e()) << "You must specify exactly one of instance directory or "
		                    "instance file!";
		return false;
	}

	if (vm.count("auto-config")) {
		this->read_auto_config(vm["auto-config"].as<std::string>());
	} else {
		BOOST_LOG(l.e()) << "You must specify auto-config!";
		return false;
	}

	if (vm.count("time-limit")) {
		cfg.set_time_limit(vm["time_limit"].as<unsigned int>());
	}

	if (vm.count("threads")) {
		cfg.set_threads(vm["threads"].as<unsigned int>());
	}

	if (vm.count("parallel")) {
		cfg.set_parallelism(vm["parallel"].as<unsigned int>());
	}

	if (vm.count("unsolved-only")) {
		cfg.set_skip_done(true);
	}
	if (vm.count("skip-oom")) {
		cfg.set_skip_oom(true);
	}

	if (vm.count("instance-seed")) {
		cfg.set_instance_seed(vm["instance-seed"].as<int>());
	}

	if (vm.count("run-id")) {
		cfg.set_run(vm["run-id"].as<std::string>());
	}

	if (vm.count("seed")) {
		cfg.set_global_seed(vm["seed"].as<int>());
	}

	if (vm.count("log-dir")) {
		cfg.set_log_dir(vm["log-dir"].as<std::string>());
	}

	if (vm.count("result-dir")) {
		cfg.set_result_dir(vm["result-dir"].as<std::string>());
	}

	if (vm.count("partition-count")) {
		cfg.set_partition_count(vm["partition-count"].as<unsigned int>());
	}
	if (vm.count("partition-number")) {
		cfg.set_partition_number(vm["partition-number"].as<unsigned int>());
	}

	if (vm.count("thread-check-time")) {
		cfg.set_thread_check_time(vm["thread-check-time"].as<double>());
	}

	return true;
}

void
AutotuneConfig::read_auto_config(std::string file)
{
	std::ifstream in_stream(file);
	std::stringstream buffer;
	buffer << in_stream.rdbuf();

	json outer_js = json::parse(buffer.str());

	std::vector<json> inner_configs;

	if (outer_js.is_array()) {
		for (json inner_js : outer_js) {
			inner_configs.push_back(inner_js);
		}
	} else {
		inner_configs.push_back(outer_js);
	}

	for (json js : inner_configs) {
		this->parameters.emplace_back();
		for (auto element_it = js.begin(); element_it != js.end(); ++element_it) {
			std::string name = element_it.key();
			json & config = element_it.value();
			std::string type = config["type"];

			if (type == "enum") {
				this->parameters.back().emplace_back(name, config["value"]);
			} else if (type == "fixed") {
				switch (config["value"].type()) {
				case json::value_t::number_unsigned:
				case json::value_t::number_integer: {
					int value = config["value"];
					this->parameters.back().emplace_back(name, value, value, 0);
					break;
				}
				case json::value_t::number_float: {
					double value = config["value"];
					this->parameters.back().emplace_back(name, value, value, 0.0);
					break;
				}
				case json::value_t::boolean: {
					bool value = config["value"];
					this->parameters.back().emplace_back(name, value, true);
					break;
				}
				case json::value_t::null:
				case json::value_t::string:
				case json::value_t::object:
				case json::value_t::array:
				case json::value_t::discarded:
				default: {
					BOOST_LOG(l.e()) << "unknown parameter value type: " + name;
					throw "unknown parameter value type: " + name;
					break;
				}
				}
			} else if (type == "linear") {
				switch (config["min"].type()) {
				case json::value_t::number_unsigned:
				case json::value_t::number_integer: {
					int min = config["min"];
					int max = config["max"];
					int step = config["step"];
					this->parameters.back().emplace_back(name, min, max, step);
					break;
				}
				case json::value_t::number_float: {
					double min = config["min"];
					double max = config["max"];
					double step = config["step"];
					this->parameters.back().emplace_back(name, min, max, step);
					break;
				}
				case json::value_t::boolean: {
					bool value = config["min"];
					this->parameters.back().emplace_back(name, value, false);
					break;
				}
				case json::value_t::null:
				case json::value_t::string:
				case json::value_t::object:
				case json::value_t::array:
				case json::value_t::discarded:
				default: {
					BOOST_LOG(l.e()) << "unknown parameter value type: " + name;
					throw "unknown parameter value type: " + name;
					break;
				}
				}
			} else {
				BOOST_LOG(l.e()) << "unknown parameter type: " + name;
				throw "unknown parameter type: " + name;
			}
		}
		std::sort(
		    this->parameters.back().begin(), this->parameters.back().end(),
		    [](Parameter & a, Parameter & b) { return a.getName() < b.getName(); });
	}

	this->current_parameter_group = 0;
}

bool
AutotuneConfig::nextConfig()
{
	bool finalState = true;
	std::vector<Parameter> * param_group =
	    &this->parameters[this->current_parameter_group];

	do {
		for (auto & p : *param_group) {
			finalState &= p.isLastValue();
		}
		if (finalState) {
			this->current_parameter_group++;
			if (this->current_parameter_group >= this->parameters.size()) {
				return false;
			}
			param_group = &this->parameters[this->current_parameter_group];
		}
	} while (finalState);

	current_config++;
	bool carry = true;
	size_t index = 0;
	while (carry && index < param_group->size()) {
		Parameter & p = param_group->at(index);
		carry = p.isLastValue();
		p.nextValue();
		index++;
	}

	return true;
}

SolverConfig
AutotuneConfig::generateConfig()
{
	json general({});
	json config({});
	Configuration & cfg = *Configuration::get();
	general["name"] = "generated config #" + std::to_string(current_config) +
	                  " of run " + cfg.get_run();
	for (auto & parameter : this->parameters[this->current_parameter_group]) {
		json value = parameter.getCurrentValue();
		for (auto element_it = value.begin(); element_it != value.end();
		     ++element_it) {
			if (element_it.key() == "regex" || element_it.key() == "time_limit" ||
			    element_it.key() == "name") {
				general[element_it.key()] = element_it.value();
			} else {
				config[element_it.key()] = element_it.value();
			}
		}
	}
	general["config"] = config;
	json configs;
	configs["solvers"] = std::vector<json>(1, general);

	return SolverConfig::read_configs(configs)[0];
}

AutotuneConfig * AutotuneConfig::instance = nullptr;
