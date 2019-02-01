//
// Created by lukas on 19.10.17.
//

#include "configuration.hpp"
#include "../datastructures/maybe.hpp"             // for Maybe
#include "log.hpp"                                 // for Log
#include "solverconfig.hpp"                        // for SolverConfig
#include <boost/lexical_cast/bad_lexical_cast.hpp> // for bad_lexic...
#include <boost/program_options.hpp>
#include <fstream>  // for operator<<
#include <iostream> // for cout
#include <string>

namespace po = boost::program_options;

Configuration::Configuration() : l("CONFIG") { this->set_defaults(); }

bool
Configuration::parse_cmdline(int argc, const char ** argv)
{
	this->set_defaults();

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
    ("algo-config,c", po::value<std::string>(), "Specifies the path to a single JSON algorithm "
                    "configuration file. This may not be used in conjunction with -a. All algorithms "
                    "specified in the config file will be executed on all instances with the specified "
                    "configurations.")
    ("algo-regexp,a", po::value<std::string>(), "Specifies a regular expression to match against"
                    " the algorithm IDs. All matching algorithms will be executed on all instances. The "
                    "algorithms must be configured with the optional parameters below. This may not be "
                    "used in conjunction with -c.")

    ("parallel,p", po::value<unsigned int>(), "Sets the number of instances that should be solved "
                    "in parallel. This is independent of the actual solver, it will create separate solver"
                    " instances. Note that depending on the actual solver, each solver might spawn "
                    "multiple threads, too. See the threads option. Defaults to 1.")
    ("time-limit,l", po::value<unsigned int>(), "Limits the maximum computation time to "
                    "<num> seconds. After that time, all solvers will be asked to quit. If -c is "
                    "used, the per-algorithm time limit supersedes this option.")
    ("memory-metrics,m", "Enables memory metrics, either via sampling RSS and DATA sizes or (if TCPSPSuite "
                    "was compiled with INSTRUMENT_MALLOC) via malloc() instrumentation. If -c is "
	            "used, the per-algorithm time limit supersedes this option.")
    ("memory-sampling-time", po::value<unsigned int>(), "Sets the time interval (in milliseconds) between "
     		    "two samplings of the DATA and RSS sizes. This is only meaningful if -m is active TCPSPSuite was "
     		    "not compiled with INSTRUMENT_MALLOC. If -c is used, the per-algorithm time limit supersedes this option.")
    ("papi-metrics", po::value<std::string>(), "PAPI metrics that should be collected, comma separated. If you have"
     		    "papi-tools installed, the papi_avail binary will tell you which metrics are available on your system. "
     		    "If -c is used, the per-algorithm time limit supersedes this option.")
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

	if (vm.count("storage")) {
		this->storage_path = vm["storage"].as<std::string>();
	} else {
		BOOST_LOG(l.e()) << "You have to specify a storage path.\n";
		return false;
	}

	if (vm.count("instance-dir")) {
		this->instance_dir = vm["instance-dir"].as<std::string>();
	}
	if (vm.count("instance-file")) {
		this->instance_file = vm["instance-file"].as<std::string>();
	}
	if (this->instance_dir.valid() == this->instance_file.valid()) {
		BOOST_LOG(l.e()) << "You must specify exactly one of instance directory or "
		                    "instance file!";
		return false;
	}

	if (vm.count("algo-config")) {
		this->algo_config_file = vm["algo-config"].as<std::string>();
	}
	if (vm.count("algo-regexp")) {
		this->algo_regexp = vm["algo-regexp"].as<std::string>();
	}
	if (this->algo_config_file.valid() == this->algo_regexp.valid()) {
		BOOST_LOG(l.e())
		    << "You must specify exactly one of algo-config or algo-regexp!";
		return false;
	}

	if (vm.count("time-limit")) {
		this->time_limit = vm["time-limit"].as<unsigned int>();
	}

	if (vm.count("threads")) {
		this->threads = vm["threads"].as<unsigned int>();
	}

	this->enable_memory_metrics = (vm.count("memory-metrics") > 0);

	if (vm.count("memory-sampling-time")) {
		this->meminfo_sampling_time = vm["mem-info"].as<unsigned int>();
	} else {
		this->meminfo_sampling_time = 500; // 500 ms default
	}

	if (vm.count("papi-metrics")) {
		std::stringstream ss(vm["papi-metrics"].as<std::string>());
		std::string item;
		while (std::getline(ss, item, ',')) {
			this->papi_metrics.push_back(item);
		}
	}

	if (vm.count("parallel")) {
		this->parallelism = vm["parallel"].as<unsigned int>();
	}

	if (vm.count("unsolved-only")) {
		this->skip_done = true;
	}

	if (vm.count("skip-oom")) {
		this->skip_oom = true;
	}

	if (vm.count("instance-seed")) {
		this->instance_seed = vm["instance-seed"].as<int>();
	}

	if (vm.count("run-id")) {
		this->run = vm["run-id"].as<std::string>();
	}

	if (vm.count("seed")) {
		this->global_seed = vm["seed"].as<int>();
	}

	if (vm.count("log-dir")) {
		this->log_dir = vm["log-dir"].as<std::string>();
	}

	if (vm.count("result-dir")) {
		this->result_dir = vm["result-dir"].as<std::string>();
	}

	if (vm.count("partition-count")) {
		this->partition_count = vm["partition-count"].as<unsigned int>();
	}
	if (vm.count("partition-number")) {
		this->partition_number = vm["partition-number"].as<unsigned int>();
	}

	if (vm.count("thread-check-time")) {
		this->thread_check_time = vm["thread-check-time"].as<double>();
	}
	
	if (this->partition_count.valid() != this->partition_number.valid()) {
		BOOST_LOG(l.e())
		    << "You must set both --partition-count and --partition-number!";
		BOOST_LOG(l.w()) << "Ignoring partition settings.";
		this->partition_count = Maybe<unsigned int>();
		this->partition_number = Maybe<unsigned int>();
	}

	return true;
}

void
Configuration::set_defaults()
{
	this->instance_dir = {};
	this->instance_file = {};
	this->algo_config_file = {};
	this->algo_regexp = {};
	this->time_limit = {};
	this->threads = 1;
	this->meminfo_sampling_time = {};
	this->parallelism = 1;
	this->run = "UNSPECIFIED";
	this->skip_done = false;
	this->skip_oom = false;
	this->instance_seed = {};
	this->global_seed = {};
	this->log_dir = {};
	this->result_dir = {};
}

void
Configuration::set_storage_path(std::string path)
{
	this->storage_path = path;
}

const std::string &
Configuration::get_storage_path() const
{
	return storage_path;
}

void
Configuration::set_instance_dir(Maybe<std::string> path)
{
	this->instance_dir = path;
}

const Maybe<std::string> &
Configuration::get_instance_dir() const
{
	return instance_dir;
}

void
Configuration::set_instance_file(Maybe<std::string> path)
{
	this->instance_file = path;
}

const Maybe<std::string> &
Configuration::get_instance_file() const
{
	return instance_file;
}

void
Configuration::set_algo_config_file(Maybe<std::string> path)
{
	this->algo_config_file = path;
}

const Maybe<std::string> &
Configuration::get_algo_config_file() const
{
	return algo_config_file;
}

void
Configuration::set_algo_regexp(Maybe<std::string> regex)
{
	this->algo_regexp = regex;
}

const Maybe<std::string> &
Configuration::get_algo_regexp() const
{
	return algo_regexp;
}

void
Configuration::set_time_limit(Maybe<unsigned int> seconds)
{
	this->time_limit = seconds;
}

const Maybe<unsigned int> &
Configuration::get_time_limit() const
{
	return time_limit;
}

void
Configuration::set_run(std::string id)
{
	this->run = id;
}

const std::string &
Configuration::get_run() const
{
	return run;
}

void
Configuration::set_instance_seed(Maybe<int> seed)
{
	this->instance_seed = seed;
}

const Maybe<int> &
Configuration::get_instance_seed() const
{
	return instance_seed;
}

void
Configuration::set_global_seed(Maybe<int> seed)
{
	this->global_seed = seed;
}

const Maybe<int> &
Configuration::get_global_seed() const
{
	return global_seed;
}

void
Configuration::set_log_dir(Maybe<std::string> path)
{
	this->log_dir = path;
}

const Maybe<std::string> &
Configuration::get_log_dir() const
{
	return log_dir;
}

void
Configuration::set_result_dir(Maybe<std::string> path)
{
	this->result_dir = path;
}

const Maybe<std::string> &
Configuration::get_result_dir() const
{
	return result_dir;
}

void
Configuration::set_skip_done(bool skip)
{
	this->skip_done = skip;
}

bool
Configuration::get_skip_done() const
{
	return this->skip_done;
}

void
Configuration::set_skip_oom(bool skip)
{
	this->skip_oom = skip;
}

bool
Configuration::get_skip_oom() const
{
	return this->skip_oom;
}

void
Configuration::set_threads(Maybe<unsigned int> t)
{
	this->threads = t;
}

Maybe<unsigned int>
Configuration::get_threads() const
{
	return this->threads;
}

bool
Configuration::are_memory_metrics_enabled() const
{
	return this->enable_memory_metrics;
}

unsigned int
Configuration::get_meminfo_sampling_time() const
{
	return this->meminfo_sampling_time;
}

const std::vector<std::string> &
Configuration::get_papi_metrics() const
{
	return this->papi_metrics;
}

void
Configuration::set_parallelism(unsigned int p)
{
	this->parallelism = p;
}

unsigned int
Configuration::get_parallelism() const
{
	return this->parallelism;
}

Maybe<unsigned int>
Configuration::get_partition_count() const
{
	return this->partition_count;
}

Maybe<unsigned int>
Configuration::get_partition_number() const
{
	return this->partition_number;
}

void
Configuration::set_partition_number(unsigned int pn)
{
	this->partition_number = pn;
}

void
Configuration::set_partition_count(unsigned int pc)
{
	this->partition_count = pc;
}

void
Configuration::set_thread_check_time(Maybe<double> seconds)
{
	this->thread_check_time = seconds;
}

Maybe<double>
Configuration::get_thread_check_time() const
{
	return this->thread_check_time;
}

Configuration * Configuration::instance = nullptr;
