#include "ilp.hpp"

#include "../algorithms/graphalgos.hpp" // for DFS
#include "../baselines/earlyscheduler.hpp"
#include "../contrib/ilpabstraction/src/common.hpp" // for ObjectiveType
#include "../db/storage.hpp"
#include "../instance/instance.hpp"
#include "../instance/laggraph.hpp" // for LagGraph
#include "../instance/resource.hpp"
#include "../instance/traits.hpp" // for Traits, Tra...
#include "../manager/errors.hpp"  // for Inconsisten...
#include "../util/configuration.hpp"
#include "../util/fault_codes.hpp" // for FAULT_INVAL...
#include "../util/log.hpp"         // for Log
#include "../util/solverconfig.hpp"
#include "generated_config.hpp" // for DOUBLE_DELTA

#include <algorithm>          // for max, min
#include <assert.h>           // for assert
#include <ext/alloc_traits.h> // for __alloc_tra...
#include <functional>         // for function
#include <limits>             // for numeric_limits
#include <memory>             // for allocator

#if defined(GUROBI_FOUND)
#include "../contrib/ilpabstraction/src/ilpa_gurobi.hpp"
#endif

#if defined(CPLEX_FOUND)
#include "../contrib/ilpabstraction/src/ilpa_cplex.hpp"
#endif

template <class SolverT>
ILPBase<SolverT>::ILPBase(const Instance & instance_in,
                          AdditionalResultStorage & additional,
                          const SolverConfig & sconf_in)
    : instance(instance_in), env(false), model(env.create_model()),
      start_points_set(false), earliest_release(0), latest_deadline(0),
      timelimit(-1), seed(0), optimized(false), collect_kappa_stats(false),
      additional_storage(additional), sconf(sconf_in),
      cb(this->timer, this->additional_storage, this->l), l("ILPBase")
{
	this->seed = sconf.get_seed();
	if (sconf.get_time_limit().valid()) {
		this->timelimit = (int)sconf.get_time_limit(); // TODO refactor this
	}

	if (sconf.has_config("initialize_with_early")) {
		this->initialize_with_early = (sconf["initialize_with_early"] == "yes");
	} else {
		this->initialize_with_early = true;
	}

	if (sconf.has_config("collect_kappa_stats")) {
		this->collect_kappa_stats = (bool)sconf["collect_kappa_stats"];
	}

	// FIXME Require: flat-resource-availability, linear-costs

	// Actually, why shouldn't it be possible to do negative lags?
}

template <class SolverT>
ILPBase<SolverT>::~ILPBase()
{
	BOOST_LOG(l.d()) << "Freeing memory";
}

template <class SolverT>
const Traits ILPBase<SolverT>::required_traits =
    Traits(0, std::numeric_limits<unsigned int>::max(), {0.0, 1.0}, {0.0, 1.0});

template <class SolverT>
const Traits &
ILPBase<SolverT>::get_requirements()
{
	return ILPBase<SolverT>::required_traits;
}

template <class SolverT>
Maybe<double>
ILPBase<SolverT>::get_lower_bound()
{
	return Maybe<double>(this->model.get_bound());
}

template <class SolverT>
void
ILPBase<SolverT>::compute_values()
{
	this->earliest_release = std::numeric_limits<unsigned int>::max();
	this->latest_deadline = std::numeric_limits<unsigned int>::min();

	for (unsigned int i = 0; i < this->instance.job_count(); i++) {
		unsigned int deadline = this->instance.get_job(i).get_deadline();
		unsigned int release = this->instance.get_job(i).get_release();

		this->latest_deadline =
		    (unsigned int)std::max(this->latest_deadline, deadline);
		this->earliest_release = std::min(this->earliest_release, release);
	}

	// Adjust for possible deadline extension
	this->latest_deadline += this->instance.get_window_extension_limit();
	this->earliest_release = (unsigned int)std::max(
	    0, (int)this->earliest_release -
	           (int)this->instance.get_window_extension_limit());
}

template <class SolverT>
void
ILPBase<SolverT>::prepare_edge_constraints()
{
	if (!this->start_points_set) {
		BOOST_LOG(l.d(1)) << "Start variables unavailable. Base ILP does not "
		                     "enforce dependencies.";
		return;
	}

	for (const auto & edge : this->instance.get_laggraph().edges()) {
		LagGraph::vertex from = edge.s, to = edge.t;
		double lag = edge.lag;

		this->model.add_constraint(this->start_points[from] + lag,
		                           this->start_points[to], MIPSolver::INFTY);
	}
}

template <class SolverT>
void
ILPBase<SolverT>::prepare_objective()
{
	Expression investment_sum = this->env.create_expression();

	for (unsigned int i = 0; i < this->instance.resource_count(); ++i) {
		const Resource & res = this->instance.get_resource(i);
		// FIXME only works for linear resources

		// Investment Costs
		polynomial investment_costs = res.get_investment_costs();
		for (poly_term term : investment_costs) {
			double coefficient = std::get<0>(term);
			double exponent = std::get<1>(term);
			if ((exponent > 1.0) || (exponent < 1.0)) {
				throw InconsistentDataError(this->instance, this->seed,
				                            FAULT_INVALID_COST_EXPONENTS,
				                            "Only 1.0 is supported as exponent");
			}
			investment_sum += coefficient * this->max_usage_variables[i];
		}
	}

	// ABSTRACTED
	// this->model->addConstr(this->overshoot_cost_variable, GRB_GREATER_EQUAL,
	// overshoot_sum);

	this->model.add_constraint(investment_sum, this->investment_cost_variable,
	                           MIPSolver::INFTY);
	// ABSTRACTED
	// this->model->addConstr(this->investment_cost_variable, GRB_GREATER_EQUAL,
	// investment_sum);

	this->model.set_objective(this->overshoot_cost_variable +
	                              this->investment_cost_variable,
	                          ilpabstraction::ObjectiveType::MINIMIZE);
}

template <class SolverT>
void
ILPBase<SolverT>::base_run()
{
	if (this->sconf.has_config("dump_path")) {
		std::string dump_path = this->sconf["dump_path"];
		this->model.write(dump_path + std::string(".lp"));
	}

	if (this->collect_kappa_stats) {
		if constexpr (decltype(this->env)::features()
		                  .template has_feature<MIPFeatures::KAPPA_STATS>()) {
			this->model.enable_kappa_statistics();
		} else {
			BOOST_LOG(l.w()) << "Solver does not support Kappa statistics.";
		}
	}

	this->model.commit_variables();
	unsigned int num_vars = this->model.get_variable_count();
	unsigned int num_constr = this->model.get_constraint_count();
	if (num_constr == 0) {
		BOOST_LOG(l.w())
		    << "MIP model reported zero constraints. This seems wrong.";
	}
	unsigned int num_nzs = this->model.get_nonzero_count();
	if (num_nzs == 0) {
		BOOST_LOG(l.w())
		    << "MIP model reported zero nonzero matrix entries. This seems wrong.";
	}

	AdditionalResultStorage::ExtendedMeasure em_vars{
	    "ILP_SIZE_VARIABLES",
	    Maybe<unsigned int>(),
	    Maybe<double>(),
	    AdditionalResultStorage::ExtendedMeasure::TYPE_INT,
	    {(int)num_vars}};
	this->additional_storage.extended_measures.push_back(em_vars);

	AdditionalResultStorage::ExtendedMeasure em_constr{
	    "ILP_SIZE_CONSTRAINTS",
	    Maybe<unsigned int>(),
	    Maybe<double>(),
	    AdditionalResultStorage::ExtendedMeasure::TYPE_INT,
	    {(int)num_constr}};
	this->additional_storage.extended_measures.push_back(em_constr);

	AdditionalResultStorage::ExtendedMeasure em_nzs{
	    "ILP_SIZE_NONZEROES",
	    Maybe<unsigned int>(),
	    Maybe<double>(),
	    AdditionalResultStorage::ExtendedMeasure::TYPE_INT,
	    {(int)num_nzs}};
	this->additional_storage.extended_measures.push_back(em_nzs);

	Maybe<unsigned int> time_limit;
	if (this->timelimit > 0) {
		time_limit = Maybe<unsigned int>(
		    (unsigned int)this->timelimit); // TODO refactor this
	}
	this->solve(time_limit);

	if (this->sconf.has_config("dump_solution_path")) {
		this->model.write_solution(this->sconf["dump_solution_path"]);
	}

	if (this->collect_kappa_stats) {
		if constexpr (decltype(this->env)::features()
		                  .template has_feature<MIPFeatures::KAPPA_STATS>()) {
			auto stats = this->model.kappa_stats();

			AdditionalResultStorage::ExtendedMeasure em_stable{
			    "ILP_KAPPA_STABLE",
			    Maybe<unsigned int>(),
			    Maybe<double>(),
			    AdditionalResultStorage::ExtendedMeasure::TYPE_INT,
			    {static_cast<int>(stats.stable.value_or(-1.0))}};
			this->additional_storage.extended_measures.push_back(em_stable);

			AdditionalResultStorage::ExtendedMeasure em_suspicious{
			    "ILP_KAPPA_SUSPICIOUS",
			    Maybe<unsigned int>(),
			    Maybe<double>(),
			    AdditionalResultStorage::ExtendedMeasure::TYPE_INT,
			    {static_cast<int>(stats.suspicious.value_or(-1.0))}};
			this->additional_storage.extended_measures.push_back(em_suspicious);

			AdditionalResultStorage::ExtendedMeasure em_unstable{
			    "ILP_KAPPA_UNSTABLE",
			    Maybe<unsigned int>(),
			    Maybe<double>(),
			    AdditionalResultStorage::ExtendedMeasure::TYPE_INT,
			    {static_cast<int>(stats.unstable.value_or(-1.0))}};
			this->additional_storage.extended_measures.push_back(em_unstable);

			AdditionalResultStorage::ExtendedMeasure em_illposed{
			    "ILP_KAPPA_ILLPOSED",
			    Maybe<unsigned int>(),
			    Maybe<double>(),
			    AdditionalResultStorage::ExtendedMeasure::TYPE_INT,
			    {static_cast<int>(stats.illposed.value_or(-1.0))}};
			this->additional_storage.extended_measures.push_back(em_illposed);

			AdditionalResultStorage::ExtendedMeasure em_kappamax{
			    "ILP_KAPPA_MAX",
			    Maybe<unsigned int>(),
			    Maybe<double>(),
			    AdditionalResultStorage::ExtendedMeasure::TYPE_INT,
			    {static_cast<int>(stats.kappamax.value_or(-1.0))}};
			this->additional_storage.extended_measures.push_back(em_kappamax);

			AdditionalResultStorage::ExtendedMeasure em_kappaattention{
			    "ILP_KAPPA_ATTENTION",
			    Maybe<unsigned int>(),
			    Maybe<double>(),
			    AdditionalResultStorage::ExtendedMeasure::TYPE_INT,
			    {static_cast<int>(stats.attention.value_or(-1.0))}};
			this->additional_storage.extended_measures.push_back(em_kappaattention);
		}
	}
}

template <class SolverT>
void
ILPBase<SolverT>::prepare_pre()
{
	this->model.set_param(ParamType::LOG_TO_CONSOLE, false);
	this->model.add_callback(&this->cb);
	this->model.set_param(ParamType::SEED, this->seed);

	if (Configuration::get()->get_threads().valid()) {
		this->model.set_param(ParamType::THREADS,
		                      Configuration::get()->get_threads().value());
	}

	if (this->sconf.has_config("focus")) {
		try {
			int focus;
			focus = this->sconf["focus"];
			BOOST_LOG(l.d(1)) << "Setting MIP focus to " << focus;

			switch (focus) {
			case 0:
				this->model.set_param(ParamType::MIP_FOCUS,
				                      ilpabstraction::ParamMIPFocus::BALANCED);
				break;
			case 1:
				this->model.set_param(ParamType::MIP_FOCUS,
				                      ilpabstraction::ParamMIPFocus::QUALITY);
				break;
			case 2:
				this->model.set_param(ParamType::MIP_FOCUS,
				                      ilpabstraction::ParamMIPFocus::BOUND);
				break;
			case 3:
				this->model.set_param(ParamType::MIP_FOCUS,
				                      ilpabstraction::ParamMIPFocus::OPTIMALITY);
				break;
			default:
				BOOST_LOG(l.e()) << "Unknown MIP focus: " << focus;
			}
		} catch (const std::exception & e) {
			BOOST_LOG(l.e()) << "Could not parse 'focus' parameter";
		}
	}

	BOOST_LOG(l.d()) << "Preparing Base Variables";
	this->prepare_base_variables();

	this->compute_values();
}

template <class SolverT>
void
ILPBase<SolverT>::generate_vars_start_points()
{
	/*
	 * Job Start Times
	 */
	this->start_points.resize(this->instance.job_count());

	for (unsigned int i = 0; i < this->instance.job_count(); i++) {
		const Job & job = this->instance.get_job(i);
		int earliest_start_with_extension =
		    std::max(((int)job.get_release() -
		              (int)this->instance.get_window_extension_limit()),
		             0);
		int latest_finish_with_extension =
		    (int)job.get_deadline() +
		    (int)this->instance.get_window_extension_limit();

		// Start points
		this->start_points[i] = this->model.add_var(
		    ilpabstraction::VariableType::INTEGER, earliest_start_with_extension,
		    latest_finish_with_extension - job.get_duration(),
		    std::string("start_point_" + std::to_string(i)));
	}

	this->start_points_set = true;
}

template <class SolverT>
void
ILPBase<SolverT>::prepare_base_variables()
{
	/*
	 * Duration Variables
	 */

	this->duration_variables.clear();
	this->duration_variables.resize(this->instance.job_count());
	for (unsigned int i = 0; i < this->instance.job_count(); i++) {
		const Job & job = this->instance.get_job(i);
		unsigned int max_overduration = 0;

		for (const auto & edge :
		     this->instance.get_laggraph().reverse_neighbors(i)) {
			max_overduration += edge.max_recharge;
		}

		// Create a duration variable
		// TODO hint this to the jobs duration
		this->duration_variables[i] = this->model.add_var(
		    ilpabstraction::VariableType::INTEGER, job.get_duration(),
		    job.get_duration() + max_overduration,
		    std::string("duration_") + std::to_string(i));
	}

	/*
	 * Resource Usage
	 */
	this->max_usage_variables.resize(this->instance.resource_count());
	for (unsigned int i = 0; i < this->instance.resource_count(); i++) {
		this->max_usage_variables[i] = this->model.add_var(
		    ilpabstraction::VariableType::CONTINUOUS, 0, MIPSolver::INFTY,
		    std::string("max_res_") + std::to_string(i));
	}

	this->overshoot_cost_variable =
	    this->model.add_var(ilpabstraction::VariableType::CONTINUOUS, 0,
	                        MIPSolver::INFTY, std::string("overshoot_costs"));

	this->investment_cost_variable =
	    this->model.add_var(ilpabstraction::VariableType::CONTINUOUS, 0,
	                        MIPSolver::INFTY, std::string("investment_costs"));
}

template <class SolverT>
void
ILPBase<SolverT>::prepare_post()
{
	BOOST_LOG(l.d()) << "Preparing Edge Constraints";
	this->prepare_edge_constraints();
	BOOST_LOG(l.d()) << "Preparing Objective";
	this->prepare_objective();
}

template <class SolverT>
void
ILPBase<SolverT>::do_initialization()
{
	if (!this->initialize_with_early) {
		return;
	}

	if (!this->start_points_set) {
		BOOST_LOG(l.d(1))
		    << "Start points not set. Cannot initialize with EarlyScheduler.";
		return;
	}

	BOOST_LOG(l.d(1)) << "Initializing with EarlyScheduler results";

	AdditionalResultStorage dummy_storage;
	EarlyScheduler es(this->instance, dummy_storage,
	                  SolverConfig("DUMMY", "DUMMY", {}, Maybe<unsigned int>(),
	                               false, 1, {},
	                               Maybe<int>(this->sconf.get_seed())));
	es.run();
	Solution es_sol = es.get_solution();

	for (unsigned int jid = 0; jid < this->instance.job_count(); ++jid) {
		this->model.set_start(this->start_points[jid], es_sol.get_start_time(jid));
	}
}

template <class SolverT>
void
ILPBase<SolverT>::solve(Maybe<unsigned int> time_limit)
{
	this->optimized = false;
	this->timer.start();

	if (time_limit.valid()) {
		BOOST_LOG(l.d(3)) << "Setting time limit: " << time_limit.value();
		this->model.set_param(ParamType::TIME_LIMIT, (int)time_limit.value());
	} else {
		BOOST_LOG(l.d(3)) << "Running without time limit.";
		// this->model.set_param(ParamType::TIME_LIMIT,
		// std::numeric_limits<int>::max());
	}

	this->do_initialization();
	this->model.solve();

	switch (this->model.get_status()) {
	case ModelStatus::OPTIMAL:
		BOOST_LOG(l.i()) << "Model optimized.";
		this->optimized = true;
		break;
	case ModelStatus::STOPPED:
		BOOST_LOG(l.w()) << "Time limit exceeded, model is not optimal.";
		break;
	case ModelStatus::INFEASIBLE:
		BOOST_LOG(l.e()) << "Model is infeasible!.";
		break;
	case ModelStatus::UNBOUNDED:
		BOOST_LOG(l.e()) << "Model is unbounded!.";
		break;
	case ModelStatus::READY:
	case ModelStatus::SOLVING:
	default:
		BOOST_LOG(l.e()) << "Unknown status code";
	}
}

template <class SolverT>
Solution
ILPBase<SolverT>::get_solution()
{
	if (this->start_points_set) {
		return this->get_solution_by_start_vars();
	} else {
		return Solution(this->instance, this->optimized,
		                this->computed_solution_start_times,
		                this->get_lower_bound());
	}
}

template <class SolverT>
Solution
ILPBase<SolverT>::get_solution_by_start_vars()
{
	std::vector<Maybe<unsigned int>> start_times;
	start_times.resize(this->instance.job_count());

	// If we did not even find a feasible solution, we can skip all this
	if (this->model.has_feasible()) {
		for (unsigned int i = 0; i < this->instance.job_count(); i++) {
			start_times[i] = (unsigned int)std::round(
			    this->model.get_variable_assignment(this->start_points[i]));
		}
	}

#ifdef ENABLE_CONSISTENCY_CHECKS
	Solution sol(this->instance, this->optimized, start_times,
	             this->get_lower_bound());

	if (this->model.has_feasible()) { // otherwise, everything is bogus
		if ((sol.get_costs() < this->model.get_objective_value() * 0.99) ||
		    (sol.get_costs() > this->model.get_objective_value() * 1.01)) {
			// TODO make this depend on the MIP gap threshold

			sol.print_jobs();
			// sol.print();
			sol.print_profile();

			// this->print_profile();
			BOOST_LOG(l.d(1)) << "Solution quality computed: " << sol.get_costs();
			BOOST_LOG(l.d(1)) << " -> Overshoot costs computed: "
			                  << sol.get_overshoot_costs();
			BOOST_LOG(l.d(1)) << " -> Investment costs computed: "
			                  << sol.get_investment_costs();
			BOOST_LOG(l.d(1)) << "Solution quality reported: "
			                  << this->model.get_objective_value();

			BOOST_LOG(l.d(1)) << " -> Overshoot costs reported: "
			                  << this->model.get_variable_assignment(
			                         this->overshoot_cost_variable);
			// this->overshoot_cost_variable.get(GRB_DoubleAttr_X) ;
			BOOST_LOG(l.d(1)) << " -> Investment costs reported: "
			                  << this->model.get_variable_assignment(
			                         this->investment_cost_variable);
			// this->investment_cost_variable.get(GRB_DoubleAttr_X) ;

			for (unsigned int r = 0; r < this->instance.resource_count(); ++r) {
				double usage =
				    this->model.get_variable_assignment(this->max_usage_variables[r]);
				BOOST_LOG(l.d(1)) << " --> Resource investment amount for " << r << ": "
				                  << usage;
			}

			throw InconsistentResultError(this->instance, this->seed,
			                              FAULT_INCONSISTENT_OBJVALS,
			                              "Inconsistent objective values");
		}
	}
	return sol;
#else
	return Solution(this->instance, this->optimized, start_times,
	                this->get_lower_bound());
#endif
}

template <class SolverT>
ILPBase<SolverT>::Callback::Callback(
    const Timer & timer_in, AdditionalResultStorage & additional_storage_in,
    Log & l_in)
    : timer(timer_in), additional_storage(additional_storage_in), last_log(0),
      lines_before_header(0), l(l_in)
{}

template <class SolverT>
void
ILPBase<SolverT>::Callback::log_intermediate()
{
	double time_point = this->timer.get();
	if (this->last_intermediate_time > time_point + LOG_INTERVAL) {
		return;
	}
	/*
	if ((this->obj_val < std::numeric_limits<double>::max()) || (this->obj_bound >
	0)) {

	        Maybe<double> obj_val_param;
	        if (this->obj_val < 1.0e100) { // This seems to be the (undocumented)
	value for "not found" obj_val_param = Maybe<double>(this->obj_val);
	        }

	        Maybe<double> obj_bound_param;
	        if (this->obj_bound > 0) {
	                obj_bound_param = Maybe<double>(this->obj_bound);
	        }

	        Maybe<double> time_param(time_point);
	        this->additional_storage.intermediate_results.push_back(
	                                {time_param, Maybe<unsigned int>(),
	                                 obj_val_param, obj_bound_param,
	                                 Maybe<Solution>()});

	        this->last_intermediate_time = time_point;
	}
	 */
}

template <class SolverT>
void
ILPBase<SolverT>::Callback::on_message(
    typename MIPSolver::Callback::Context & ctx, std::string & message)
{
	(void)ctx;
	(void)message;
	// BOOST_LOG(l.i()) << message;
}

template <class SolverT>
void
ILPBase<SolverT>::Callback::on_poll(typename MIPSolver::Callback::Context & ctx)
{
	if (this->timer.get() > this->last_log + LOG_INTERVAL) {
		double obj_val = this->get_objective_value(ctx);
		double obj_bound = this->get_bound(ctx);
		int n_nodes_done = this->get_processed_nodes(ctx);
		int n_nodes_open = this->get_open_nodes(ctx);
		double time = this->timer.get();
		double gap = this->get_gap(ctx);

		if (lines_before_header < 1) {
			BOOST_LOG(l.i()) << "===================================================="
			                    "===============================";
			BOOST_LOG(l.i()) << "|| " << std::setw(8) << " Time "
			                 << " || " << std::setw(10) << "Incumbent"
			                 << " | " << std::setw(10) << "Best Bound"
			                 << " | " << std::setw(6) << "Gap"
			                 << " || " << std::setw(10) << "Nodes Done"
			                 << " | " << std::setw(10) << "Nodes Open"
			                 << " || ";
			BOOST_LOG(l.i()) << "===================================================="
			                    "===============================";

			lines_before_header = REPEAT_HEADER;
		}

		if ((obj_val >= 0) && (obj_bound >= 0) && (n_nodes_done >= 0) &&
		    (n_nodes_open >= 0)) {
			BOOST_LOG(l.i()) << "|| " << std::setw(6) << std::setprecision(0)
			                 << std::fixed << time << " s || " << std::setw(10)
			                 << std::setprecision(2) << std::fixed << obj_val << " | "
			                 << std::setw(10) << std::setprecision(2) << std::fixed
			                 << obj_bound << " | " << std::setw(6)
			                 << std::setprecision(4) << std::fixed << gap << " || "
			                 << std::setw(10) << n_nodes_done << " | "
			                 << std::setw(10) << n_nodes_open << " || ";
			lines_before_header--;
			this->last_log = this->timer.get();
		}
	}
}

// explicit instantiation

#if defined(GUROBI_FOUND)
template class ILPBase<ilpabstraction::GurobiInterface>;
#endif

#if defined(CPLEX_FOUND)
template class ILPBase<ilpabstraction::CPLEXInterface>;
#endif
