//
// Created by lukas on 14.12.17.
//

#include "parallelizer.hpp"
#include "../datastructures/maybe.hpp" // for Maybe
#include "../db/storage.hpp"
#include "../instance/instance.hpp"                    // for Instance
#include "../io/jsonreader.hpp"                        // for JsonReader
#include "../util/configuration.hpp"                   // for Configuration
#include "../util/git.hpp"                             // for GIT_SHA1
#include "../util/randomizer.hpp"                      // for Randomizer
#include "../util/solverconfig.hpp"                    // for SolverConfig
#include "selector.hpp"                                // for Selector
#include <algorithm>                                   // for min, sort
#include <boost/log/core/record.hpp>                   // for record
#include <boost/log/detail/attachable_sstream_buf.hpp> // for basic_ostring...
#include <boost/log/sources/record_ostream.hpp>        // for basic_record_...

#include "generated_config.hpp"

#ifdef NUMA_OPTIMIZE
#include <numa.h>
#endif


Parallelizer::Parallelizer(Storage & storage_in, std::string run_id_in,
                           Randomizer & randomizer_in)
    : storage(storage_in), run_id(run_id_in), randomizer(randomizer_in),
      l("PARALLELIZER")
{}

void
Parallelizer::run_in_parallel(const std::vector<std::string> & filenames,
                              const std::vector<SolverConfig> & configurations,
                              unsigned int thread_count)
{
	this->remaining_tasks.reserve(configurations.size() * filenames.size());

	for (const std::string & fname : filenames) {
		for (const SolverConfig & config : configurations) {
			this->remaining_tasks.emplace_back(fname, config);
		}
	}

	/*
	 * Handle partitioning
	 */
	auto cfg = Configuration::get();
	if (cfg->get_partition_number().valid()) {
		BOOST_LOG(l.i()) << "Selecting only partition "
		                 << cfg->get_partition_number() << " of "
		                 << cfg->get_partition_count();

		size_t partition_size =
		    this->remaining_tasks.size() / cfg->get_partition_count();
		// Correct for integer division error
		if ((this->remaining_tasks.size() % cfg->get_partition_count()) > 0) {
			partition_size += 1;
		}

		auto low_it = this->remaining_tasks.begin() +
		              (partition_size * cfg->get_partition_number());
		auto high_it =
		    this->remaining_tasks.begin() +
		    std::min<size_t>(
		        this->remaining_tasks.size(),
		        (size_t)(partition_size * (cfg->get_partition_number() + 1)));

		auto cmp = [](const auto & lhs, const auto & rhs) {
			return (std::hash<std::string>{}(lhs.first) ^
			        std::hash<SolverConfig>{}(lhs.second)) <
			       (std::hash<std::string>{}(rhs.first) ^
			        std::hash<SolverConfig>{}(rhs.second));
		};

		std::nth_element(this->remaining_tasks.begin(), low_it,
		                 this->remaining_tasks.end(), cmp);
		std::nth_element(low_it, high_it, this->remaining_tasks.end(), cmp);

		std::vector<std::pair<std::string, SolverConfig>> my_partition(low_it,
		                                                               high_it);
		this->remaining_tasks = std::move(my_partition);
	}

	this->totalTasks = remaining_tasks.size();

	for (unsigned int i = 0; i < thread_count; ++i) {
		this->threads.push_back(std::thread(&Parallelizer::run_thread, this, i));
	}

	for (auto & thread : this->threads) {
		thread.join();
	}
}

Maybe<std::pair<std::string, SolverConfig>>
Parallelizer::get_next_task()
{
	std::lock_guard<std::mutex> lock(this->queue_mutex);

	if (this->remaining_tasks.empty()) {
		return Maybe<std::pair<std::string, SolverConfig>>();
	} else {
		size_t done = this->totalTasks - remaining_tasks.size();
		BOOST_LOG(l.i()) << "====================================================";
		BOOST_LOG(l.i()) << "ca. " << done << " of " << this->totalTasks
		                 << " tasks finished";
		BOOST_LOG(l.i()) << remaining_tasks.size() << " tasks remaining";
		BOOST_LOG(l.i()) << "====================================================";
		std::pair<std::string, SolverConfig> next_task =
		    this->remaining_tasks.back();
		this->remaining_tasks.pop_back();
		return {next_task};
	}
}

void
Parallelizer::run_thread(int thread_id)
{
	Selector selector(this->storage, this->run_id, thread_id);

	Maybe<std::pair<std::string, SolverConfig>> task = this->get_next_task();

#ifdef NUMA_OPTIMIZE
	assert(numa_available() != -1);

	BOOST_LOG(l.i()) << "Pinning to NUMA node "
	                 << (thread_id % (numa_max_node() + 1));

	numa_run_on_node(thread_id % (numa_max_node() + 1));
	numa_set_membind(numa_get_run_node_mask());
#else
	(void)thread_id;
#endif
	// TODO set the thread ID in the logger!

	while (task.valid()) {
		std::string file_name = task.value().first;
		JsonReader reader(file_name);
		Instance * instance = reader.parse();
		SolverConfig & solverConfig = task.value().second;

		BOOST_LOG(l.i()) << "====================================================";
		BOOST_LOG(l.i()) << "Now processing: ";
		BOOST_LOG(l.i()) << file_name;
		BOOST_LOG(l.i()) << solverConfig.get_name();
		BOOST_LOG(l.i()) << "====================================================";
		BOOST_LOG(l.d(1)) << "Software version: " << std::string(GIT_SHA1);

		BOOST_LOG(l.d(2)) << " Config Settings: ";
		for (auto & [k, v] : solverConfig.get_kvs()) {
			BOOST_LOG(l.d(2)) << "   " << k << ": \t" << v;
		}

		BOOST_LOG(l.d(1)) << "====================================================";

		instance->compute_traits();

		Configuration & cfg = *Configuration::get();
		if (cfg.get_instance_seed().valid()) {
			solverConfig.override_seed(cfg.get_instance_seed().value());
		}
		if (!solverConfig.was_seed_set()) {
			solverConfig.override_seed(this->randomizer.get_random());
		}

		selector.run_all(*instance, solverConfig);

		// TODO FIXME re-add logging!
		/*
		if (log_dir != nullptr) {
		    Log::remove_logger(file_log);
		    delete file_log;
		}
		*/

		delete instance;

		task = this->get_next_task();
	}
}
