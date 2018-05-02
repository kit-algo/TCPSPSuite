#ifndef SELECTOR_H
#define SELECTOR_H

#include "generated_config.hpp"

#include "solvers.hpp"
#include "solver_headers.hpp"

#include "../baselines/earlyscheduler.hpp"
#include "../util/randomizer.hpp"
#include "../util/solverconfig.hpp"
#include "runner.hpp"

#include <boost/hana.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/core/to.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>
namespace hana = boost::hana;

class Selector {
public:
  inline Selector(Storage &storage, std::string run_id);
  inline void run_all(const Instance &instance, const SolverConfig & solver_cfg);
private:
  Storage & storage;
  std::string run_id;

  Log l;
};

#include "selector.cpp"

#endif
