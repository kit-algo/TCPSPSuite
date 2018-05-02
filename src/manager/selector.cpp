#include "selector.hpp"

#include "util/log.hpp"
#include "util/configuration.hpp"

Selector::Selector(Storage &storage_in, std::string run_id_in)
  : storage(storage_in), run_id(run_id_in), l("SELECTOR")
{}

void
Selector::run_all(const Instance & instance, const SolverConfig & solver_cfg)
{
  // Get all registered solvers
  // retrieve the maximum N
  constexpr unsigned int max_N = solvers::get_free_N<void>() - 1;
  // get the set of registered solvers
  auto registered_solvers = solvers::registry_hook<max_N>{}();

  BOOST_LOG(l.d()) << "::::: Matching against: " << solver_cfg.get_id();

  hana::for_each(registered_solvers, [&](auto solver_cls) {
    BOOST_LOG(l.d()) << "::: Trying solver: " << decltype(solver_cls)::type::get_id() ;

    if (solver_cfg.match(decltype(solver_cls)::type::get_id())) {
      BOOST_LOG(l.d()) << "::: Got a match, executing.";
      Runner<typename decltype(solver_cls)::type> runner(this->storage, this->run_id, solver_cfg);

      BOOST_LOG(l.d()) << ":::: Instance seed: " << solver_cfg.get_seed();

      runner.run(instance);
    }
  });
}