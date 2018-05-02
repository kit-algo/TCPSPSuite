//
// Created by lukas on 14.12.17.
//

#include "parallelizer.hpp"

#include "../io/jsonreader.hpp"
#include "selector.hpp"
#include "generated_config.hpp"
#include "../util/git.hpp"                                    // for GIT_SHA1
#include "util/configuration.hpp"

Parallelizer::Parallelizer(Storage & storage_in, std::string run_id_in, Randomizer & randomizer_in)
    : storage(storage_in), run_id(run_id_in), randomizer(randomizer_in), l("PARALLELIZER")
{}

void
Parallelizer::run_in_parallel(const std::vector<std::string> &filenames, const std::vector<SolverConfig> & configurations, unsigned int thread_count)
{
    for (const SolverConfig & config : configurations) {
        for (const std::string & fname : filenames) {
            this->remaining_tasks.emplace(fname, config);
        }
    }

    for (unsigned int i = 0 ; i < thread_count ; ++i) {
        this->threads.push_back(std::thread(&Parallelizer::run_thread, this));
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
        std::pair<std::string, SolverConfig> next_task = this->remaining_tasks.front();
        this->remaining_tasks.pop();
        return { next_task };
    }
}

void
Parallelizer::run_thread()
{
    Selector selector(this->storage, this->run_id);

    Maybe<std::pair<std::string, SolverConfig>> task = this->get_next_task();

    while (task.valid()) {
        std::string file_name = task.value().first;
        JsonReader reader(file_name);
        Instance *instance = reader.parse();
        SolverConfig& solverConfig = task.value().second;

        BOOST_LOG(l.i()) << "====================================================";
        BOOST_LOG(l.i()) << "Now processing: ";
        BOOST_LOG(l.i()) << file_name;
        BOOST_LOG(l.i()) << solverConfig.get_name();
        BOOST_LOG(l.d(1)) << "Software version: " << std::string(GIT_SHA1);
        BOOST_LOG(l.i()) << "====================================================";

        instance->compute_traits();
        
        Configuration & cfg = * Configuration::get();
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