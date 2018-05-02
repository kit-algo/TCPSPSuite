//
// Created by lukas on 14.12.17.
//

#ifndef TCPSPSUITE_PARALLELIZER_HPP
#define TCPSPSUITE_PARALLELIZER_HPP

#include <thread>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <utility>

#include "../db/storage.hpp"
#include "../util/randomizer.hpp"
#include "../util/solverconfig.hpp"
#include "../datastructures/maybe.hpp"

class Parallelizer {
public:
    Parallelizer(Storage &storage, std::string run_id, Randomizer & randomizer);
    void run_in_parallel(const std::vector<std::string> & filenames, const std::vector<SolverConfig> & configurations, unsigned int thread_count);
private:
    Storage & storage;
    std::string run_id;
    Randomizer & randomizer;

    void run_thread();
    Maybe<std::pair<std::string, SolverConfig>> get_next_task();

    std::mutex queue_mutex;
    std::queue<std::pair<std::string, SolverConfig>> remaining_tasks;

    std::vector<std::thread> threads;

    Log l;
};

#endif //TCPSPSUITE_PARALLELIZER_HPP
