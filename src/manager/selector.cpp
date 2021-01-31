#include "selector.hpp"

#include "../util/configuration.hpp"
#include "../util/log.hpp"
#include "../util/thread_checker.hpp"

Selector::Selector(Storage & storage_in, std::string run_id_in,
                   int thread_id_in)
    : storage(storage_in), run_id(run_id_in), thread_id(thread_id_in),
      l("SELECTOR")
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
		BOOST_LOG(l.d()) << "::: Trying solver: "
		                 << decltype(solver_cls)::type::get_id();

		if (solver_cfg.match(decltype(solver_cls)::type::get_id())) {
			util::ThreadChecker & tchk = util::ThreadChecker::get();
			tchk.check(this->thread_id);

			BOOST_LOG(l.d()) << "::: Got a match, executing.";
			bool bad_alloc = false;

			try {
				Runner<typename decltype(solver_cls)::type> runner(
				    this->storage, this->run_id, solver_cfg);

				BOOST_LOG(l.d()) << ":::: Instance seed: " << solver_cfg.get_seed();

				runner.run(instance);
			} catch (std::bad_alloc & ba) {
				// We can't do anything here that requires more memory,
				// so we just set the flag
				bad_alloc = true;
			}
#ifdef GUROBI_FOUND
			catch (GRBException & grb_ex) {
				if (grb_ex.getErrorCode() == GRB_ERROR_OUT_OF_MEMORY) {
					BOOST_LOG(l.w()) << "Gurobi went out of memory!";
					bad_alloc = true;
				} else {
					BOOST_LOG(l.e()) << "Gurobi error: " << grb_ex.getMessage() << "\n";
					throw;
				}
			}
#endif

			if (bad_alloc) {
				BOOST_LOG(l.e()) << " !!!!!!! OUT OF MEMORY !!!!!!!!";

				this->storage.insert_error(instance.get_id(), this->run_id,
				                           decltype(solver_cls)::type::get_id(),
				                           solver_cfg.get_name(), solver_cfg.get_seed(),
				                           0u, FAULT_OUT_OF_MEMORY);
			}
		}
	});
}
