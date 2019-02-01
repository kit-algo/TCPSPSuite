#include "runner.hpp"

#include "errors.hpp"
#include "instance/solution.hpp"
#include "instance/traits.hpp"
#include "instance/transform.hpp"
#include "io/solutionwriter.hpp"
#include "memoryinfo.hpp"
#include "timer.hpp"
#include "util/configuration.hpp"
#include "util/fault_codes.hpp"
#include "visualization/dotfile.hpp"

template <class Solver>
Runner<Solver>::Runner(Storage & storage_in, std::string run_id_in,
                       const SolverConfig & sconf_in)
    : storage(storage_in), sconf(sconf_in), run_id(run_id_in), l("RUNNER")
{}

template <class Solver>
void
Runner<Solver>::run(const Instance & instance_in)
{
	Instance instance = instance_in.clone(); // mutable copy!

	try {
		BOOST_LOG(l.i()) << "Deriving transformation path…";
		TraitsRouter tr(TransformerManager::get().get_all());

		BOOST_LOG(l.d(2)) << "Trying to route from: " << instance.get_traits();
		BOOST_LOG(l.d(2)) << "Trying to route to: " << Solver::get_requirements();

		// DotfileExporter dfe(instance);
		// dfe.write("/tmp/before.dot");

		Maybe<std::vector<Transformer *>> tpath =
		    tr.get_path(instance.get_traits(), Solver::get_requirements());
		if (!tpath.valid()) {
			BOOST_LOG(l.e()) << "Could not determine transformation path.";
			throw InconsistentDataError(instance, sconf.get_seed(),
			                            FAULT_NO_TRANSFORMATION,
			                            "No transformation "
			                            "path found");
		} else {
			for (auto transformer : tpath.value()) {
				transformer->run(instance);
				instance = transformer->get_transformed();
			}
		}

		// DotfileExporter dfe_after(instance);
		// dfe_after.write("/tmp/after.dot");

	} catch (const RuntimeError & exception) {
		// We don't have a solver yet - initialize with non-transformed instance
		AdditionalResultStorage aresults;
		Solver solver(instance, aresults, sconf);
		ErrorHandler handler(this->storage, solver.get_id(), this->run_id,
		                     sconf.get_name(), &sconf);
		handler.handle(exception);
		return;
	}

	AdditionalResultStorage aresults;

	Solver solver(instance, aresults, sconf);

	Solution sol; // Must be declared here s.t. it can be read in the exception
	              // handler!

	try {
		if ((Configuration::get()->get_skip_done()) &&
		    (storage.check_result(instance.get_id(), this->run_id, solver.get_id(),
		                          sconf))) {
			// We already have that result. Log and continue.
			BOOST_LOG(l.i()) << "Result already in database, aborting.";
			return;
		}

		if ((Configuration::get()->get_skip_oom()) &&
		    (storage.check_error({}, {FAULT_OUT_OF_MEMORY}, instance.get_id(),
		                         this->run_id, solver.get_id(), sconf))) {
			BOOST_LOG(l.w()) << "Not computing: Found previous out-of-memory error!";
			return;
		}

		Timer t;

		unsigned int meminfo_time = sconf.get_meminfo_sampling_time();
		manager::LinuxMemoryInfo mem_info(meminfo_time);
		manager::PAPIPerformanceInfo * papi_ptr = nullptr;

#ifdef PAPI_FOUND
		manager::PAPIPerformanceInfo papi_info(sconf.get_papi_metrics());
		if (Configuration::get()->get_parallelism() <= 1) {
			papi_ptr = &papi_info;
		}
#endif

		t.start();
		if (sconf.are_memory_metrics_enabled()) {
			if (Configuration::get()->get_parallelism() > 1) {
				BOOST_LOG(l.w())
				    << "Cannot collect memory statistics in parallel mode!";
			} else {
				mem_info.start();
			}
		}

#ifdef PAPI_FOUND
		if (sconf.get_papi_metrics().size() > 0) {
			if (Configuration::get()->get_parallelism() > 1) {
				BOOST_LOG(l.w())
				    << "Cannot collect PAPI measurements in parallel mode!";
			} else {
				papi_info.start();
			}
		}
#endif

		solver.run();
		sol = solver.get_solution();
		double elapsed = t.stop();

		if (Configuration::get()->get_parallelism() <= 1) {
#ifdef PAPI_FOUND
			if (sconf.get_papi_metrics().size() > 0) {
				papi_info.stop();
			}
#endif
			if (sconf.are_memory_metrics_enabled()) {
				mem_info.stop();
			}
		}

		BOOST_LOG(l.i()) << "Solution costs: " << sol.get_costs();

		// TODO verify against unmodified instance?
		if (sol.is_feasible()) {
			InconsistentResultError err(instance, 0, 0, std::string(""));
			if (!sol.verify(sconf.get_seed(), &err)) {
				throw err;
			}
		}

		std::string solver_id = solver.get_id();
		long unsigned int res_id;
		if (sconf.are_memory_metrics_enabled() &&
		    (sconf.get_papi_metrics().size() > 0)) {
			res_id = storage.insert(sol, this->run_id, solver_id, sconf.get_name(),
			                        sconf.get_seed(), elapsed, sconf, aresults,
			                        &mem_info, papi_ptr);
		} else {
			res_id = storage.insert(sol, this->run_id, solver_id, sconf.get_name(),
			                        sconf.get_seed(), elapsed, sconf, aresults,
			                        nullptr, papi_ptr);
		}

		if (Configuration::get()->get_result_dir().valid()) {
			std::string filename = Configuration::get()->get_result_dir();
			filename += std::string("/") + instance.get_id() + std::string("___") +
			            solver.get_id() + std::string("___") +
			            std::to_string(this->sconf.get_seed()) + std::string(".json");
			BOOST_LOG(l.i()) << "Writing result to " << filename;

			SolutionWriter writer(sol, {res_id});
			writer.write_to(filename);
		}

	} catch (const RuntimeError & exception) {
		if ((Configuration::get()->get_result_dir().valid()) &&
		    (sol.get_instance() != nullptr)) {
			std::string filename = Configuration::get()->get_result_dir();
			filename += std::string("/") + instance.get_id() + std::string("___") +
			            solver.get_id() + std::string("___") +
			            std::to_string(this->sconf.get_seed()) +
			            std::string("-ERRORED.json");
			BOOST_LOG(l.e()) << "Writing ERRORED result to " << filename;

			SolutionWriter writer(sol, Maybe<unsigned long int>());
			writer.write_to(filename);
		}

		ErrorHandler handler(this->storage, solver.get_id(), this->run_id,
		                     sconf.get_name(), &sconf);
		handler.handle(exception);
		return;
	}
}
