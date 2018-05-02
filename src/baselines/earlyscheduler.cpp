#include "earlyscheduler.hpp"
#include <limits>                     // for numeric_limits
#include "../algorithms/graphalgos.hpp"  // for CriticalPathComputer
#include "../instance/traits.hpp"        // for Traits, Traits::LAGS_DAG, Trait...
class AdditionalResultStorage;
class Instance;
class SolverConfig;

const Traits EarlyScheduler::required_traits = Traits(
    Traits::LAGS_ONLY_POSITIVE | Traits::LAGS_DAG | Traits::NO_DRAIN ,
    std::numeric_limits<unsigned int>::max(),
    {}, {});

EarlyScheduler::EarlyScheduler(const Instance &instance_in, AdditionalResultStorage & additional, const SolverConfig & sconf)
  : instance(instance_in), l("EARLY")
{
  (void)additional;
  (void)sconf;
}

const Traits &
EarlyScheduler::get_requirements()
{
  return EarlyScheduler::required_traits;
}

void
EarlyScheduler::run()
{
  CriticalPathComputer cpc(this->instance);
  this->earliest_starts = cpc.get_forward();
}

Maybe<double>
EarlyScheduler::get_lower_bound()
{
  return Maybe<double>();
}

Solution
EarlyScheduler::get_solution()
{
  return Solution(this->instance, false, this->earliest_starts, Maybe<double>());
}

std::string
EarlyScheduler::get_id()
{
  return "EarlyScheduler v1";
}
