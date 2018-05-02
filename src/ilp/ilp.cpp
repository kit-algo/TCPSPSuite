#include "ilp.hpp"
#include <assert.h>                                       // for assert
#include <ext/alloc_traits.h>                             // for __alloc_tra...
#include <algorithm>                                      // for max, min
#include <functional>                                     // for function
#include <limits>                                         // for numeric_limits
#include <memory>                                         // for allocator
#include "../algorithms/graphalgos.hpp"                   // for DFS
#include "../instance/laggraph.hpp"                       // for LagGraph
#include "../manager/errors.hpp"                          // for Inconsisten...
#include "../util/fault_codes.hpp"                        // for FAULT_INVAL...
#include "../util/log.hpp"                                // for Log
#include "generated_config.hpp"                           // for DOUBLE_DELTA
#include "../contrib/ilpabstraction/src/common.hpp"   // for ObjectiveType
#include "../instance/traits.hpp"                     // for Traits, Tra...
#include "../db/storage.hpp"
#include "../instance/instance.hpp"
#include "../instance/resource.hpp"
#include "../util/solverconfig.hpp"
#include "../util/configuration.hpp"
#include "../baselines/earlyscheduler.hpp"

#if defined(GUROBI_FOUND)
#include "../contrib/ilpabstraction/src/ilpa_gurobi.hpp"
#endif

#if defined(CPLEX_FOUND)
#include "../contrib/ilpabstraction/src/ilpa_cplex.hpp"
#endif


template <class SolverT>
ILPBase<SolverT>::ILPBase(const Instance &instance_in, AdditionalResultStorage & additional, const
                      SolverConfig & sconf_in) :
	instance(instance_in), env(false), model(env.create_model()), timelimit(-1),
	additional_storage(additional), sconf(sconf_in), cb(this->timer, this->additional_storage,
	                                                    this->l), l("ILPBase")
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
	// FIXME Require: flat-resource-availability, linear-costs

	// Actually, why shouldn't it be possible to do negative lags?
}

template <class SolverT>
ILPBase<SolverT>::~ILPBase()
{
	BOOST_LOG(l.d()) << "Freeing memory";
}

template <class SolverT>
const Traits ILPBase<SolverT>::required_traits = Traits(
	Traits::LAGS_ONLY_POSITIVE,
	std::numeric_limits<unsigned int>::max(),
	{0.0, 1.0}, {0.0, 1.0});

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

	for (unsigned int i = 0 ; i < this->instance.job_count() ; i++) {
		unsigned int deadline = this->instance.get_job(i).get_deadline();
		unsigned int release = this->instance.get_job(i).get_release();

		this->latest_deadline = (unsigned int)std::max(this->latest_deadline, deadline);
		this->earliest_release = std::min(this->earliest_release, release);
	}

	// Adjust for possible deadline extension
	this->latest_deadline +=  this->instance.get_window_extension_limit();
	this->earliest_release = (unsigned int)std::max(0, (int)this->earliest_release
	                                      - (int)this->instance.get_window_extension_limit());
}

template <class SolverT>
void
ILPBase<SolverT>::prepare_extension_constraints()
{
	/*
	 * Limit the sum of the extensions / number of extended jobs
	 */
	Expression extension_sum = this->env.create_expression();
	Expression modified_sum = this->env.create_expression();
	for (unsigned int i = 0; i < this->instance.job_count(); ++i) {
		extension_sum += this->left_extension_var[i];
		extension_sum += this->right_extension_var[i];
		modified_sum += (1 - this->window_not_modified_var[i]);
	}

	this->model.add_constraint(extension_sum, this->window_extension_time_var, MIPSolver::INFTY);
	this->model.add_constraint(modified_sum, this->window_extension_job_var, MIPSolver::INFTY);

	this->window_extension_time_constraint =
		this->model.add_constraint(MIPSolver::NEGATIVE_INFTY, this->window_extension_time_var,
		                           this->instance.get_window_extension_limit());
	this->window_extension_job_constraint =
		this->model.add_constraint(MIPSolver::NEGATIVE_INFTY, this->window_extension_job_var,
		                           this->instance.get_window_extension_job_limit());

	/*
	 * Force unmodified-var to zero
	 */
	for (unsigned int i = 0; i < this->instance.job_count(); ++i) {
		this->model.add_sos1_constraint({this->left_extension_var[i], this->right_extension_var[i],
		                                 this->window_not_modified_var[i]}, {});
	}
}

template <class SolverT>
void
ILPBase<SolverT>::prepare_edge_constraints()
{
	for (const auto &edge : this->instance.get_laggraph().edges()) {
		LagGraph::vertex from = edge.s, to = edge.t;
		double lag = edge.lag;

		this->model.add_constraint(this->start_points[from] + lag, this->start_points[to],
															 MIPSolver::INFTY);
		// ABSTRACTED
		//GRBConstr constr =
		//this->model->addConstr(this->start_points[to], GRB_GREATER_EQUAL, this->start_points[from] +
		//			  lag);

		//std::cout << "Constraint for " << to << " after " << from << " (lag " << lag << "): "  << "\n";
	}
}

template <class SolverT>
void
ILPBase<SolverT>::prepare_objective() {
	Expression overshoot_sum = this->env.create_expression();
	Expression investment_sum = this->env.create_expression();

	for (unsigned int i = 0 ; i < this->instance.resource_count() ; ++i) {
		const Resource &res =  this->instance.get_resource(i);
		// FIXME only works for linear resources

		// Investment Costs
		polynomial investment_costs = res.get_investment_costs();
		for (poly_term term : investment_costs) {
			double coefficient = std::get<0>(term);
			double exponent = std::get<1>(term);
			if ((exponent > 1.0) || (exponent < 1.0)) {
				throw InconsistentResultError(this->instance, this->seed, FAULT_INVALID_COST_EXPONENTS, "Only 1.0 is supported as exponent");
			}
			investment_sum += coefficient * this->max_usage_variables[i];
		}

		// Overshoot costs
		polynomial overshoot_costs = res.get_overshoot_costs();
		for (poly_term term : overshoot_costs) {
			double coefficient = std::get<0>(term);
			double exponent = std::get<1>(term);
			if ((exponent > 1.0) || (exponent < 1.0)) {
				throw InconsistentResultError(this->instance, this->seed, FAULT_INVALID_COST_EXPONENTS, "Only 1.0 is supported as exponent");
			}

			for (unsigned int t = 0 ; t <= this->latest_deadline ; ++t) {
				overshoot_sum += coefficient * this->overshoot_variables[i][t];
			}
		}
	}

	this->model.add_constraint(overshoot_sum, this->overshoot_cost_variable, MIPSolver::INFTY);
	// ABSTRACTED
	//this->model->addConstr(this->overshoot_cost_variable, GRB_GREATER_EQUAL, overshoot_sum);

	this->model.add_constraint(investment_sum, this->investment_cost_variable, MIPSolver::INFTY);
	// ABSTRACTED
	//this->model->addConstr(this->investment_cost_variable, GRB_GREATER_EQUAL, investment_sum);

	this->model.set_objective(this->overshoot_cost_variable + this->investment_cost_variable,
														ilpabstraction::ObjectiveType::MINIMIZE);
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
					this->model.set_param(ParamType::MIP_FOCUS, ilpabstraction::ParamMIPFocus::BALANCED);
					break;
				case 1:
					this->model.set_param(ParamType::MIP_FOCUS, ilpabstraction::ParamMIPFocus::QUALITY);
					break;
				case 2:
					this->model.set_param(ParamType::MIP_FOCUS, ilpabstraction::ParamMIPFocus::BOUND);
					break;
				case 3:
					this->model.set_param(ParamType::MIP_FOCUS, ilpabstraction::ParamMIPFocus::OPTIMALITY);
					break;
				default:
					BOOST_LOG(l.e()) << "Unknown MIP focus: " << focus;
			}
		} catch (std::exception e) {
			BOOST_LOG(l.e()) << "Could not parse 'focus' parameter";
		}
	}
}

template <class SolverT>
void
ILPBase<SolverT>::prepare_post()
{
	BOOST_LOG(l.d()) << "Preparing Extension Constraints";
	this->prepare_extension_constraints();
}

template<class SolverT>
void
ILPBase<SolverT>::do_initialization()
{
	if (!this->initialize_with_early) {
		return;
	}

	BOOST_LOG(l.d(1)) << "Initializing with EarlyScheduler results";

	AdditionalResultStorage dummy_storage;
	EarlyScheduler es(this->instance, dummy_storage,
	                  SolverConfig("DUMMY", "DUMMY", {}, Maybe<unsigned int>(),
	                               Maybe<int>(this->sconf.get_seed())));
	es.run();
	Solution es_sol = es.get_solution();

	for (unsigned int jid = 0 ; jid < this->instance.job_count() ; ++jid) {
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
		this->model.set_param(ParamType::TIME_LIMIT, std::numeric_limits<int>::max());
	}

	this->do_initialization();
	this->model.solve();

	switch (this->model.get_status()) {
	case ModelStatus::OPTIMAL:
		BOOST_LOG(l.i()) << "Model optimized." ;
		this->optimized = true;
		break;
	case ModelStatus::STOPPED:
		BOOST_LOG(l.w()) << "Time limit exceeded, model is not optimal." ;
		break;
	case ModelStatus::INFEASIBLE:
		BOOST_LOG(l.e()) << "Model is infeasible!." ;
		break;
	case ModelStatus::UNBOUNDED:
		BOOST_LOG(l.e()) << "Model is unbounded!." ;
		break;
	case ModelStatus::READY:
	case ModelStatus::SOLVING:
	default:
		BOOST_LOG(l.e()) << "Unknown status code";
	}
}

template <class SolverT>
std::string
ILPBase<SolverT>::get_id()
{
	return "ILPBase v2.3 (" + std::string(SolverT::NAME) + ")";
}

template <class SolverT>
void
ILPBase<SolverT>::print_profile() const
{
	std::vector<double> sums(this->instance.resource_count(), 0.0);

	for (unsigned int t = 0 ; t < this->latest_deadline ; t++) {
		BOOST_LOG(l.d(2)) << "====> Step " << t ;
		for (unsigned int r = 0 ; r < this->instance.resource_count() ; ++r) {
			double overshoot = this->model.get_variable_assignment(this->overshoot_variables[r][t]);
			//double overshoot = this->overshoot_variables[r][t].get(GRB_DoubleAttr_X);
			sums[r] += overshoot;
			BOOST_LOG(l.d(2)) << "    Res " << r << " overshoot: " << overshoot ;
		}
	}

	for (unsigned int r = 0 ; r < this->instance.resource_count() ; ++r) {
		BOOST_LOG(l.d(2)) << "======> Overshoot sum for Res " << r << ": " << sums[r] ;
	}
}

template <class SolverT>
Solution
ILPBase<SolverT>::get_solution()
{
	std::vector<Maybe<unsigned int>> start_times;
	start_times.resize(this->instance.job_count());

	// If we did not even find a feasible solution, we can skip all this
	if (this->model.has_feasible()) {
		for (unsigned int i = 0 ; i < this->instance.job_count() ; i++) {
			start_times[i] = (unsigned int)std::round(
							this->model.get_variable_assignment(this->start_points[i]));
		}
	}

#ifdef ENABLE_CONSISTENCY_CHECKS
	Solution sol(this->instance, this->optimized, start_times, this->get_lower_bound());

	if (this->model.has_feasible()) { // otherwise, everything is bogus
		if ((sol.get_costs() < this->model.get_objective_value() * 0.99) ||
				(sol.get_costs() > this->model.get_objective_value() * 1.01)) {
			// TODO make this depend on the MIP gap threshold

			sol.print_jobs();
			//sol.print();
			//sol.print_profile();

			//this->print_profile();
			BOOST_LOG(l.d(1)) << "Solution quality computed: " << sol.get_costs() ;
			BOOST_LOG(l.d(1)) << " -> Overshoot costs computed: " << sol.get_overshoot_costs();
			BOOST_LOG(l.d(1)) << " -> Investment costs computed: " << sol.get_investment_costs();
			BOOST_LOG(l.d(1)) << "Solution quality reported: " << this->model.get_objective_value() ;

			BOOST_LOG(l.d(1)) << " -> Overshoot costs reported: "
												<< this->model.get_variable_assignment(this->overshoot_cost_variable);
			//this->overshoot_cost_variable.get(GRB_DoubleAttr_X) ;
			BOOST_LOG(l.d(1)) << " -> Investment costs reported: "
												<< this->model.get_variable_assignment(this->investment_cost_variable);
			//this->investment_cost_variable.get(GRB_DoubleAttr_X) ;

			for (unsigned int r = 0 ; r < this->instance.resource_count() ; ++r) {
				double usage = this->model.get_variable_assignment(this->max_usage_variables[r]);
				BOOST_LOG(l.d(1)) << " --> Resource investment amount for " << r << ": " << usage ;
			}

			throw InconsistentResultError(this->instance, this->seed, FAULT_INCONSISTENT_OBJVALS, "Inconsistent objective values");
		}
	}
	return sol;
#else
	return Solution(this->instance, this->optimized, start_times, this->get_lower_bound());
#endif
}

template <class SolverT>
ILPBase<SolverT>::Callback::Callback(const Timer & timer_in, AdditionalResultStorage & additional_storage_in, Log & l_in)
	: timer(timer_in), additional_storage(additional_storage_in), last_log(0), lines_before_header(0),
				l(l_in)
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
	if ((this->obj_val < std::numeric_limits<double>::max()) || (this->obj_bound > 0)) {

		Maybe<double> obj_val_param;
		if (this->obj_val < 1.0e100) { // This seems to be the (undocumented) value for "not found"
			obj_val_param = Maybe<double>(this->obj_val);
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
ILPBase<SolverT>::Callback::on_message(typename MIPSolver::Callback::Context & ctx, std::string &message)
{
	(void)ctx;
	(void)message;
	//BOOST_LOG(l.i()) << message;
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
			BOOST_LOG(l.i()) <<
			                 "===================================================================================";
			BOOST_LOG(l.i()) << "|| "
			                 << std::setw(8) << " Time " << " || "
			                 << std::setw(10) << "Incumbent" << " | "
			                 << std::setw(10) << "Best Bound" << " | "
										   << std::setw(6) << "Gap" << " || "
			                 << std::setw(10) << "Nodes Done" << " | "
								       << std::setw(10) << "Nodes Open" << " || ";
			BOOST_LOG(l.i()) << "===================================================================================";

			lines_before_header = REPEAT_HEADER;
		}

		if ((obj_val >= 0) && (obj_bound >= 0) && (n_nodes_done >= 0) && (n_nodes_open >= 0)) {
			BOOST_LOG(l.i()) << "|| "
			                 << std::setw(6) << std::setprecision(0) << std::fixed << time << " s || "
			                 << std::setw(10) << std::setprecision(2) << std::fixed  << obj_val << " | "
							         << std::setw(10) << std::setprecision(2) << std::fixed  << obj_bound << " | "
							         << std::setw(6) << std::setprecision(4) << std::fixed  << gap << " || "
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