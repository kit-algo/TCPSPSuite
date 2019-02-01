//
// Created by lukas on 14.12.17.
//

#ifndef TCPSPSUITE_PARALLELIZER_HPP
#define TCPSPSUITE_PARALLELIZER_HPP

#include "../datastructures/maybe.hpp" // for Maybe
#include "../util/log.hpp"             // for Log
#include "timer.hpp"
#include <mutex>                       // for mutex
#include <stddef.h>                    // for size_t
#include <string>                      // for string
#include <thread>                      // for thread
#include <utility>                     // for pair
#include <vector>                      // for vector
class Randomizer;
class SolverConfig;
class Storage;

class Parallelizer {
public:
  Parallelizer(Storage & storage, std::string run_id, Randomizer & randomizer);
  void run_in_parallel(const std::vector<std::string> & filenames,
                       const std::vector<SolverConfig> & configurations,
                       unsigned int thread_count);

private:
  Storage & storage;
  std::string run_id;
  Randomizer & randomizer;
  size_t totalTasks;

  void run_thread(int thread_id);
  Maybe<std::pair<std::string, SolverConfig>> get_next_task();

  std::mutex queue_mutex;
  std::vector<std::pair<std::string, SolverConfig>> remaining_tasks;

  std::vector<std::thread> threads;

  Log l;
};

#endif // TCPSPSUITE_PARALLELIZER_HPP
