#ifndef RUNNER_H
#define RUNNER_H

#include "instance/instance.hpp"
#include "util/solverconfig.hpp"
#include "db/storage.hpp"

template <class Solver>
class Runner {
public:
  Runner(Storage &storage, std::string run_id, const SolverConfig & sconf);
  void run(const Instance &instance);
private:
  Storage &storage;
  const SolverConfig & sconf;
  std::string run_id;

  Log l;
};

// Template instantiation
#include "runner.cpp"

#endif
