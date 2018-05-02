#ifndef EARLYSCHEDULER_HPP
#define EARLYSCHEDULER_HPP

#include "../datastructures/maybe.hpp"  // for Maybe
#include <string>                          // for string
#include <vector>                          // for vector
#include "../instance/solution.hpp"     // for Solution
#include "../util/log.hpp"                    // for Log
#include "../manager/solvers.hpp"

class AdditionalResultStorage;
class Instance;
class SolverConfig;
class Traits;

/**
 * @brief A Solver for a TCPSP instance
 *
 * This solver creates a topologically sorting of all jobs.
 * It tries to run every job as early as possible
 */
class EarlyScheduler {
public:
  /**
   * Constructs a new solver for a TCPSP instance
   *
   * @param instance_in  The TCPSP instance that should be solved
   * @param additional   storage for additional data (unused)
   * @param sconf        The configuration of this Solver (unused)
   */
  EarlyScheduler(const Instance &instance, AdditionalResultStorage & additional, const SolverConfig & sconf);
  
  /**
   * Calculates the result of this solver
   */
  void run();
  
  /**
   * Returns the solution of this Solver
   *
   * @return the solution of this Solver
   */
  Solution get_solution();

  /**
   * Returns the unique name of the solver
   *
   * @return the unique name of the solver
   */
  static std::string get_id();

  /**
   * Returns the lower bound found by this solver
   *
   * @return the lower bound found by this solver
   */
  Maybe<double> get_lower_bound();

  /**
   * Returns the traits required by this solver
   *
   * @return the required traits
   */
  static const Traits &get_requirements();

private:
  const Instance &instance;
  std::vector<unsigned int> earliest_starts;

  // TODO not sure about LAGS_DAG...
  static const Traits required_traits;

  Log l;
};

// Register the solver
namespace solvers {
template <>
struct registry_hook<solvers::get_free_N<EarlyScheduler>()>
{
  constexpr static unsigned int my_N = solvers::get_free_N<EarlyScheduler>();

  auto
  operator()()
  {
    return solvers::register_class < EarlyScheduler, my_N > {}();
  }
};
}

#endif
