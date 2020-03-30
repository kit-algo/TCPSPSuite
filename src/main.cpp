
#include "main.hpp"

#include "contrib/tinydir.h"        // for tinydir_next
#include "datastructures/maybe.hpp" // for Maybe
#include "db/storage.hpp"           // for Storage
#include "generated_config.hpp"     // for GUROBI_FOUND
#include "manager/parallelizer.hpp" // for Parallelizer
#include "util/configuration.hpp"   // for Configura...
#include "util/log.hpp"             // for Log
#include "util/randomizer.hpp"      // for Randomizer
#include "util/solverconfig.hpp"    // for SolverConfig

#include <algorithm>                            // for find
#include <assert.h>                             // for assert
#include <bits/exception.h>                     // for exception
#include <boost/log/core/record.hpp>            // for record
#include <boost/log/sources/record_ostream.hpp> // for basic_rec...
#include <cstdlib>                              // for exit, abort
#include <exception>                            // for rethrow_e...
#include <execinfo.h>                           // for backtrace
#include <iostream>                             // for operator<<
#include <stdexcept>                            // for runtime_e...
#include <stdio.h>                              // for fprintf
#include <string>                               // for string
#include <vector>                               // for vector
#if defined(GUROBI_FOUND)
#include <gurobi_c++.h> // for GRBException
#endif
#if defined(CPLEX_FOUND)
#define IL_STD 1
#include <ilconcert/ilosys.h>
#endif


#define BACKTRACE_SIZE 20
void * backtrace_buffer[BACKTRACE_SIZE];

class ArgumentException : public std::runtime_error {
public:
	explicit ArgumentException(const char * what) : std::runtime_error(what) {}
};

char *
get_cmd_option(char ** begin, char ** end, const std::string & option)
{
	char ** itr = std::find(begin, end, option);
	if (itr != end && ++itr != end) {
		return *itr;
	}
	return nullptr;
}

bool
has_cmd_option(char ** begin, char ** end, const std::string & option)
{
	char ** itr = std::find(begin, end, option);
	if (itr != end) {
		return true;
	}
	return false;
}

void
print_help()
{
	std::cout << "Usage: tcpsp -s <storage> [ -f <instance file> | -d <instance "
	             "directory> ] OPTIONS \n";
	std::cout << "\n";
	std::cout << "At least three options are mandatory:\n\n";
	std::cout << "  -s <storage>: Specifies the path to a sqlite3 database which "
	             "will store\n";
	std::cout << "                the results of the computations. The database "
	             "will be created\n";
	std::cout << "                if it does not exist.\n";
	std::cout << "\n";
	std::cout << "  -d <instance directory>\n";
	std::cout
	    << "              : Specifies the path to a directory containing JSON\n";
	std::cout
	    << "                instance files. The directory will be recursively\n";
	std::cout << "                scanned for JSON files. All instances found "
	             "will be\n";
	std::cout << "                computed on one after the other. May not be "
	             "specified\n";
	std::cout << "                if the -f option (see below) is used.\n";
	std::cout << "\n";
	std::cout << "  -f <instance file>\n";
	std::cout << "              : Specifies the path to a single JSON instance "
	             "file. Only this\n";
	std::cout << "                instance will be solved. May not be used if -d "
	             "is used.\n";
	std::cout << "\n";
	std::cout << "  -c <algorithm config file>\n";
	std::cout << "              : Specifies the path to a single JSON algorithm "
	             "configuration\n";
	std::cout << "                file. This may not be used in conjunction with "
	             "-a. All algorithms\n";
	std::cout << "                specified in the config file will be executed "
	             "on all instances with the\n";
	std::cout << "                specified configurations.\n";
	std::cout << "\n";
	std::cout << "  -a <algorithm regexp>\n";
	std::cout << "              : Specifies a regular expression to match "
	             "against the algorithm IDs. All\n";
	std::cout << "                matching algorithms will be executed on all "
	             "instances. The algorithms\n";
	std::cout << "                must be configured with the optional "
	             "parameters below. This may not be used\n";
	std::cout << "                in conjunction with -c.\n";
	std::cout << "\n\n";
	std::cout << "Additionally, there are optional parameters: \n\n";
	std::cout << "  -l <seconds>: Limits the maximum computation time to "
	             "<seconds> seconds. After \n";
	std::cout << "                that time, all solvers will be asked to quit. "
	             "If -c is used, the per-algorithm\n";
	std::cout << "                time limit supersedes this option.\n";
	std::cout << "\n";
	std::cout << "  -t <count>  : Sets the number of threads that solvers should "
	             "use, if they can use multithreading. \n";
	std::cout << "                Defaults to 1.\n";
	std::cout << "\n";
	std::cout << "  -r <run ID> : Sets the run ID, which may be any string. This "
	             "string can then later \n";
	std::cout << "                be used to associate the results with a "
	             "specific run.\n";
	std::cout << "\n";
	std::cout << "  -u          : Asks the software to compute only results "
	             "which are not yet in the database. \n";
	std::cout << "                An instance / solver combination is skipped if "
	             "this combination was already\n";
	std::cout << "                computed for the specified run (see -r).\n";
	std::cout << "\n";
	std::cout << "  -i <seed>   : Forces the 'instance seed' to be <seed>. Note "
	             "that if you use this in combination\n";
	std::cout << "                with -d (see above), all found instances will "
	             "be computed with the exact same seed!\n";
	std::cout << "                This is useful for reproducing specific "
	             "results. If -c is used, the per-algorithm\n";
	std::cout << "                instance seed may supersede this option.\n";
	std::cout << "\n";
	std::cout << "  -g <seed>   : Forces the 'global seed' to be <seed>. All "
	             "instance seeds are generated from this global\n";
	std::cout << "                seed. This is useful to deterministically "
	             "reproduce a whole run of the suite.\n";
	std::cout << "\n";
	std::cout << "  -o <logdir> : Sets <logdir> as path to the log directory. If "
	             "this is given, the suite will write a \n";
	std::cout << "                separate log for every solver run into this "
	             "directory.\n";
	std::cout << "\n";
	std::cout << "  -x <resdir> : Sets <resdir> as path to the result dump "
	             "directory. If this is given, the suite will write a \n";
	std::cout << "                separate JSON file for every algorithm run on "
	             "every instance, dumping the result in JSON format.\n";
	std::cout << "\n\nExamples:\n\n";
	std::cout << " Read all instances from /tmp/instances, write results to "
	             "/tmp/db.sqlite:\n";
	std::cout << "   > tcpspsuite -d /tmp/instances -s /tmp/db.sdlite\n";
	std::cout << "\n";
	std::cout << " Read a single instance from /tmp/instance.json, write results "
	             "to /tmp/db.sqlite, name the run 'testrun':\n";
	std::cout
	    << "   > tcpspsuite -f /tmp/instance.json -s /tmp/db.sdlite -r testrun\n";
	std::cout << "\n";
	std::cout << " Read all instances from /tmp/instances, write results to "
	             "/tmp/db.sqlite, stop solving after 15 minutes and do\n";
	std::cout << " not duplicate results:\n";
	std::cout
	    << "   > tcpspsuite -d /tmp/instances -s /tmp/db.sdlite -l 900 -u\n";
	std::cout << "\n";
}

void
handle_uncaught()
{
	std::cout << std::flush;
	std::cerr << std::flush;

	auto exc = std::current_exception();

	try {
		std::rethrow_exception(exc);
	} catch (const ArgumentException &) {
		print_help();
		exit(-1);
	} catch (...) {
		// do nothing
	}

	std::cout << "============= WHOOPS ================\n";
	std::cout << "Hi, I'm your friendly crash handler.\n";
	std::cout
	    << "It looks like an exception was thrown that has not been caught.\n";
	std::cout << "I'm trying to print it now:\n\n";

	std::cout << "#########################\n";
	try {
		std::rethrow_exception(exc);
	} catch (const std::exception & e) {
		std::cout << e.what() << '\n';
	}
	// TODO move this to ilp.cpp
#if defined(GUROBI_FOUND)
	catch (const GRBException & e) {
		std::cout << e.getMessage() << '\n';
	}
#endif
#if defined(CPLEX_FOUND)
	catch (const IloException & e) {
		std::cout << e << '\n';
	}
#endif
	catch (const char * e) {
		std::cout << e << "\n";
	} catch (...) {
		std::cout << "Sorry, unknown exception type.\n";
	}

	std::cout << "#########################\n\n";

	std::cout << "Depending on the sanity of your compiler and libc, I will "
	             "be\nable to show you a backtrace of the point where \nthe "
	             "exception was thrown.\n";
	std::cout << "If the addr2line tool is avaialble on your system, I might "
	             "even\nbe able to resolve the code lines.\n";
	std::cout << "Here we go:\n\n";
	std::cout << "============ BACKTRACE ===============\n";

	size_t trace_size = (size_t)backtrace(backtrace_buffer, BACKTRACE_SIZE);

	/*
	 * The following has in large parts been borrowed from
	 * https://stackoverflow.com/questions/3151779/how-its-better-to-invoke-gdb-from-program-to-print-its-stacktrace/4611112#4611112
	 *
	 * And... oh god is it hacky!
	 */

	char ** messages = backtrace_symbols(backtrace_buffer, (int)trace_size);
	/* skip first stack frame (points here) */
	fprintf(stdout, "[bt] Execution path:\n");
	for (size_t i = 1; i < trace_size; ++i) {
		fprintf(stdout, "[bt] #%zu %s\n", i, messages[i]);

		/* find first occurence of '(' or ' ' in message[i] and assume
		 * everything before that is the file name. (Don't go beyond 0 though
		 * (string terminator)*/
		size_t p = 0;
		while (messages[i][p] != '(' && messages[i][p] != ' ' &&
		       messages[i][p] != 0)
			++p;

		char syscom[256];
		int precision = (int)p;
		sprintf(syscom, "addr2line %p -e %.*s", backtrace_buffer[i], precision,
		        messages[i]);
		// last parameter is the file name of the symbol
		int result = system(syscom);
		assert(result == 0);
	}

	std::cerr << "============ BACKTRACE ===============\n";
	std::cerr << "\n";
	std::cerr << "Hope that helped. Have a nice day.\n";
	std::abort();
}

void
signal_handler(int s)
{
	std::cout << " Received signal " << s;
	std::exit(-1);
}

int
main(int argc, const char ** argv)
{
#ifdef CATCH_EXCEPTIONS
	std::set_terminate(&handle_uncaught);
#endif

#ifdef EXIT_ON_SIGINT
	struct sigaction sigIntHandler;

	sigIntHandler.sa_handler = signal_handler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;

	sigaction(SIGINT, &sigIntHandler, NULL);
#endif

	// Set up console logging
	Log::setup();
	Log l("MAIN");

	BOOST_LOG(l.i()) << "Starting up.";

	Configuration & cfg = *Configuration::get();
	if (!cfg.parse_cmdline(argc, argv)) {
		return 1;
	}

	Storage::initialize(cfg.get_storage_path(), argc, argv);

	Randomizer randomizer(cfg.get_global_seed());
	BOOST_LOG(l.d()) << "Global seed is: " << randomizer.get_global_seed();

	if (cfg.get_algo_regexp().valid()) {
		BOOST_LOG(l.d(3)) << "Adding Ad-Hoc SC with regexp "
		                  << std::string(cfg.get_algo_regexp());
		SolverConfig sc("ADHOC", std::string(cfg.get_algo_regexp()), {},
		                cfg.get_time_limit(), cfg.are_memory_metrics_enabled(),
		                cfg.get_meminfo_sampling_time(), cfg.get_papi_metrics(),
		                cfg.get_instance_seed());
		cfg.set_solver_config(sc);
	} else {
		cfg.read_solver_config(cfg.get_algo_config_file());
	}
	BOOST_LOG(l.d(3)) << "We have " << cfg.solver_configs().size()
	                  << " solver configs.";

	// const char * log_dir = get_cmd_option(argv, argv + argc, "-o");

	Storage store(cfg.get_storage_path());
	// Selector selector(store, cfg.get_run(), randomizer);

	std::vector<std::string> instances;

	if (cfg.get_instance_file().valid()) {
		instances.push_back(cfg.get_instance_file());
	} else {
		tinydir_dir dir;
		std::vector<std::string> directories;
		directories.push_back(cfg.get_instance_dir().value());

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
						instances.push_back(filename);
						BOOST_LOG(l.i()) << "Instance found: " << filename;
					}
				}

				tinydir_next(&dir);
			}
		}
	}

	Parallelizer para(store, cfg.get_run(), randomizer);
	para.run_in_parallel(instances, cfg.solver_configs(), cfg.get_parallelism());

	BOOST_LOG(l.i()) << "Finished normally";
	return 0;
}
