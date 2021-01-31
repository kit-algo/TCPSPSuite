//
// Created by lukas on 07.12.17.
//

#include "dtilp.hpp"

#include "../db/storage.hpp"
#include "../instance/instance.hpp" // IWYU pragma: keep
#include "../instance/job.hpp"
#include "../instance/laggraph.hpp"
#include "../instance/resource.hpp"
#include "../manager/errors.hpp"   // for Inconsisten...
#include "../util/fault_codes.hpp" // for FAULT_INVAL...
#include "../util/solverconfig.hpp"
#include "generated_config.hpp"

#include <limits>

#if defined(GUROBI_FOUND)
#include "../contrib/ilpabstraction/src/ilpa_gurobi.hpp"
#endif

#if defined(CPLEX_FOUND)
#include "../contrib/ilpabstraction/src/ilpa_cplex.hpp"
#endif

#include <boost/progress.hpp>

template <class SolverT>
DTILP<SolverT>::DTILP(const Instance & instance_in,
                      AdditionalResultStorage & additional,
                      const SolverConfig & sconf_in)
    : ILPBase<SolverT>(instance_in, additional, sconf_in), l("DTILP")
{
	this->use_sos1_for_starts = this->sconf.has_config("use_sos1_for_starts") &&
	                            this->sconf.as_bool("use_sos1_for_starts");
}

template <class SolverT>
void
DTILP<SolverT>::prepare_warmstart()
{
	this->prepare();
	this->job_is_fixed = std::vector<bool>(this->instance.job_count(), false);
}

template <class SolverT>
void
DTILP<SolverT>::prepare_start_point_constraints()
{
	for (unsigned int jid = 0; jid < this->instance.job_count(); ++jid) {
		Expression expr = this->env.create_expression();
		for (unsigned int t = 0; t < this->time_step_bounds[jid].second; ++t) {
			expr += (this->time_step_bounds[jid].first + t) * this->variables[jid][t];
		}

		this->model.add_constraint(expr, this->start_points[jid], expr);
	}
}

template <class SolverT>
void
DTILP<SolverT>::prepare_overshoot_costs()
{
	Expression overshoot_sum = this->env.create_expression();

	for (unsigned int i = 0; i < this->instance.resource_count(); ++i) {
		const Resource & res = this->instance.get_resource(i);
		// Overshoot costs
		for (unsigned int t = 0; t <= this->latest_deadline; ++t) {
			polynomial overshoot_costs = res.get_overshoot_costs(t);
			for (poly_term term : overshoot_costs) {
				double coefficient = std::get<0>(term);
				double exponent = std::get<1>(term);

				if ((exponent > 1.0) || (exponent < 1.0)) {
					throw InconsistentResultError(this->instance, this->seed,
					                              FAULT_INVALID_COST_EXPONENTS,
					                              "Only 1.0 is supported as exponent");
				}

				overshoot_sum += coefficient * this->overshoot_variables[i][t];
			}
		}
	}

	this->model.add_constraint(overshoot_sum, this->overshoot_cost_variable,
	                           MIPSolver::INFTY);
}

template <class SolverT>
void
DTILP<SolverT>::prepare_variables()
{
	// We use the start-variables from the base ILP
	this->generate_vars_start_points();

	// Switch-On-Variables
	this->variables.resize(this->instance.job_count());
	this->time_step_bounds.resize(this->instance.job_count());

	this->overduration_and_swon_variables.resize(this->instance.job_count());

	std::vector<double> sos1_weights;

	for (unsigned int i = 0; i < this->instance.job_count(); i++) {
		const Job & job = this->instance.get_job(i);
		int earliest_start_with_extension =
		    std::max(((int)job.get_release() -
		              (int)this->instance.get_window_extension_limit()),
		             0);
		int latest_finish_with_extension =
		    std::min((int)job.get_deadline() +
		                 (int)this->instance.get_window_extension_limit(),
		             (int)this->instance.get_window_extension_hard_deadline()
		                 .value_or_default(std::numeric_limits<int>::max()));

		this->time_step_bounds[i] = {earliest_start_with_extension,
		                             latest_finish_with_extension -
		                                 earliest_start_with_extension -
		                                 (int)job.get_duration() + 1};

		this->variables[i].resize(this->time_step_bounds[i].second);
		for (unsigned int j = 0; j < this->time_step_bounds[i].second; j++) {
			this->variables[i][j] = this->model.add_var(
			    ilpabstraction::VariableType::BINARY, 0, 1,
			    std::to_string(i) + std::string("_at_") +
			        std::to_string(this->time_step_bounds[i].first + j));
			// ABSTRACTED
			// this->variables[i][j] = this->model->addVar(0, 1, 0, GRB_BINARY,
			// std::string("start_") + 		    std::to_string(i) + std::string("_at_")
			// +
			// std::to_string(j));
		}

		// Add SOS1
		if (sos1_weights.size() < this->time_step_bounds[i].second) {
			auto old_size = sos1_weights.size();
			sos1_weights.resize(this->time_step_bounds[i].second);
			for (auto j = old_size; j < this->time_step_bounds[i].second; ++j) {
				sos1_weights[j] = (double)j;
			}
		} else if (sos1_weights.size() > this->time_step_bounds[i].second) {
			sos1_weights.resize(this->time_step_bounds[i].second);
		}
		if (this->use_sos1_for_starts) {
			this->model.add_sos1_constraint(this->variables[i], sos1_weights,
			                                std::string("sos1_") + std::to_string(i));
		}
	}

	// Duration variables
	/*
	this->duration_variables.resize(this->instance.job_count());
	for (unsigned int i = 0 ; i < this->instance.job_count() ; i++) {
	  const Job & job = this->instance.get_job(i);

	  this->duration_variables[i] =
	this->model.add_var(ilpabstraction::VariableType::INTEGER, job.get_duration(),
	                                                    MIPSolver::INFTY,
	                                                    std::string("duration_") +
	std::to_string(i));
	  // ABSTRACTED
	  //this->duration_variables[i] = this->model->addVar(0,
	std::numeric_limits<int>::max(), 0,
	  //                                                   GRB_INTEGER,
	std::string("duration_") +
	  //			  std::to_string(i));
	}
	*/

	// Duration & Overduration variables
	this->overduration_variables.resize(this->instance.job_count());
	for (unsigned int i = 0; i < this->instance.job_count(); i++) {
		unsigned int max_overduration = 0;

		for (const auto & edge :
		     this->instance.get_laggraph().reverse_neighbors(i)) {
			max_overduration += edge.max_recharge;
		}

		if (max_overduration > 0) {
			// Create Overduration indicators
			for (unsigned int t = 1; t <= max_overduration; ++t) {
				this->overduration_variables[i].push_back(this->model.add_var(
				    ilpabstraction::VariableType::BINARY, 0, 1,
				    std::string("overduration_") + std::to_string(t) +
				        std::string("_ofjob_") + std::to_string(i)));
			}

			// every t is combined with at most <max_overduration>
			// overduration-indicators
			this->overduration_and_swon_variables[i].resize(
			    this->time_step_bounds[i].second);
			for (unsigned int t = 0; t < this->time_step_bounds[i].second; ++t) {
				for (unsigned int o = 0; o < max_overduration; ++o) {
					this->overduration_and_swon_variables[i][t].push_back(
					    this->model.add_var(
					        ilpabstraction::VariableType::BINARY, 0, 1,
					        std::string("overduration_") + std::to_string(o) +
					            std::string("_AND_start_at_") +
					            std::to_string(this->time_step_bounds[i].first + t) +
					            std::string("_job_") + std::to_string(i)));
				}
			}
		}
	}

	this->overshoot_variables.resize(this->instance.resource_count());
	for (unsigned int i = 0; i < this->instance.resource_count(); i++) {
		this->overshoot_variables[i].resize((unsigned int)this->latest_deadline +
		                                    1);
		for (unsigned int j = 0; j <= this->latest_deadline; j++) {
			this->overshoot_variables[i][j] = this->model.add_var(
			    ilpabstraction::VariableType::CONTINUOUS, 0, MIPSolver::INFTY,
			    std::string("res_") + std::to_string(i) + std::string("_timestep_") +
			        std::to_string(j));
		}
	}

	/*
	 * Extension variables
	 */
	this->left_extension_var.resize(this->instance.job_count());
	this->right_extension_var.resize(this->instance.job_count());
	this->window_not_modified_var.resize(this->instance.job_count());
	for (unsigned int i = 0; i < this->instance.job_count(); i++) {
		this->left_extension_var[i] = this->model.add_var(
		    ilpabstraction::VariableType::INTEGER, 0, MIPSolver::INFTY,
		    std::string("left_extension_") + std::to_string(i));
		this->right_extension_var[i] = this->model.add_var(
		    ilpabstraction::VariableType::INTEGER, 0, MIPSolver::INFTY,
		    std::string("right_extension_") + std::to_string(i));
		this->window_not_modified_var[i] = this->model.add_var(
		    ilpabstraction::VariableType::BINARY, 0, 1,
		    std::string("window_not_modified_") + std::to_string(i));
	}
	this->window_extension_time_var = this->model.add_var(
	    ilpabstraction::VariableType::CONTINUOUS, 0, MIPSolver::INFTY,
	    std::string("total_extension_time"));
	this->window_extension_job_var =
	    this->model.add_var(ilpabstraction::VariableType::CONTINUOUS, 0,
	                        MIPSolver::INFTY, std::string("total_extension_job"));

	this->model.commit_variables();
}

template <class SolverT>
void
DTILP<SolverT>::prepare_extension_constraints()
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

	this->model.add_constraint(extension_sum, this->window_extension_time_var,
	                           MIPSolver::INFTY);
	this->model.add_constraint(modified_sum, this->window_extension_job_var,
	                           MIPSolver::INFTY);

	this->window_extension_time_constraint = this->model.add_constraint(
	    MIPSolver::NEGATIVE_INFTY, this->window_extension_time_var,
	    this->instance.get_window_extension_limit());
	this->window_extension_job_constraint = this->model.add_constraint(
	    MIPSolver::NEGATIVE_INFTY, this->window_extension_job_var,
	    this->instance.get_window_extension_job_limit());

	/*
	 * Force unmodified-var to zero
	 */
	for (unsigned int i = 0; i < this->instance.job_count(); ++i) {
		this->model.add_sos1_constraint({this->left_extension_var[i],
		                                 this->right_extension_var[i],
		                                 this->window_not_modified_var[i]},
		                                {});
	}
}

template <class SolverT>
void
DTILP<SolverT>::prepare_resource_constraints()
{
	std::vector<std::vector<Expression>> usages;
	usages.resize(this->instance.resource_count());
	for (unsigned int rid = 0; rid < this->instance.resource_count(); ++rid) {
		usages[rid].resize(this->latest_deadline);
		for (unsigned int i = 0; i < this->latest_deadline; ++i) {
			usages[rid][i] = this->env.create_expression();
		}
	}

	for (unsigned int jid = 0; jid < this->instance.job_count(); ++jid) {
		const Job & job = this->instance.get_job(jid);

		for (unsigned int rid = 0; rid < this->instance.resource_count(); ++rid) {

			if (job.get_resource_usage(rid) <= 0) {
				continue;
			}

			for (unsigned int t = 0; t < this->time_step_bounds[jid].second; ++t) {

				// Handle normal duration of the job if started at t
				for (unsigned int t_offset = 0; t_offset < job.get_duration();
				     ++t_offset) {
					usages[rid][this->time_step_bounds[jid].first + t + t_offset] +=
					    this->variables[jid][t] * job.get_resource_usage(rid);
				}

				// Handle overduration
				for (unsigned int od_length = 0;
				     od_length < this->overduration_variables[jid].size();
				     ++od_length) {
					usages[rid][this->time_step_bounds[jid].first + t +
					            job.get_duration() + od_length] +=
					    this->overduration_and_swon_variables[jid][t][od_length] *
					    job.get_resource_usage(rid);
				}
			}
		}
	}

	for (unsigned int rid = 0; rid < this->instance.resource_count(); ++rid) {
		Variable capacity = this->max_usage_variables[rid];
		const Resource & res = this->instance.get_resource(rid);
		auto availability_it = res.get_availability().begin();

		for (unsigned int t = 0; t < this->latest_deadline; ++t) {
			if ((availability_it + 1) != (res.get_availability().end()) &&
			    (availability_it + 1)->first <= t) {
				availability_it++;
			}
			double available = availability_it->second;

			this->model.add_constraint(
			    usages[rid][t] - available, this->overshoot_variables[rid][t],
			    MIPSolver::INFTY,
			    std::string("res_") + std::to_string(rid) + "_at_" +
			        std::to_string(t) + std::string("_overshoot"));
			this->model.add_constraint(
			    usages[rid][t] - available, capacity, MIPSolver::INFTY,
			    std::string("res_") + std::to_string(rid) + "_at_" +
			        std::to_string(t) + std::string("_investment"));
		}
	}
}

template <class SolverT>
void
DTILP<SolverT>::prepare_job_constraints()
{
	for (unsigned int job_no = 0; job_no < this->instance.job_count(); ++job_no) {
		Expression expr = this->env.create_expression();
		const Job & job = this->instance.get_job(job_no);
		unsigned int deadline = job.get_deadline();
		unsigned int release = job.get_release();

		for (unsigned int t = 0; t < this->time_step_bounds[job_no].second; ++t) {
			expr += this->variables[job_no][t];
		}

		this->model.add_constraint(1, expr, 1,
		                           std::string("pull_up_constraint_") +
		                               std::to_string(job_no));

		// ABSTRACTED
		// this->model->addConstr(expr, GRB_EQUAL, 1,
		// std::string("pull_up_constraint_") + 			  std::to_string(job_no));

		Expression release_expr = this->env.create_expression();
		release_expr += release;
		release_expr -= this->left_extension_var[job_no];
		this->model.add_constraint(
		    release_expr, this->start_points[job_no], INFINITY,
		    std::string("release_constraint_") + std::to_string(job_no));

		Expression deadline_expr = this->env.create_expression();
		deadline_expr += deadline;
		deadline_expr += this->right_extension_var[job_no];
		this->model.add_constraint(
		    0, this->start_points[job_no] + this->duration_variables[job_no],
		    deadline_expr,
		    std::string("deadline_constraint_") + std::to_string(job_no));
	}
}

template <class SolverT>
void
DTILP<SolverT>::prepare()
{
	this->prepare_pre();

	BOOST_LOG(l.d()) << "Preparing Variables";
	this->prepare_variables();
	BOOST_LOG(l.d()) << "Preparing Start Point Constraints";
	this->prepare_start_point_constraints();
	BOOST_LOG(l.d()) << "Preparing Job Constraints";
	this->prepare_job_constraints();
	BOOST_LOG(l.d()) << "Preparing Duration Constraints";
	this->prepare_duration_constraint();
	BOOST_LOG(l.d()) << "Preparing Resource Constraints";
	// BOOST_LOG(l.d()) << std::flush;
	this->prepare_resource_constraints();
	BOOST_LOG(l.d()) << "Preparing Extension Constraints";
	this->prepare_extension_constraints();
	BOOST_LOG(l.d()) << "Preparing overshoot costs";
	this->prepare_overshoot_costs();

	this->prepare_post();
}

// TODO re-write this. It's a mess.
template <class SolverT>
void
DTILP<SolverT>::prepare_duration_constraint()
{
	for (unsigned int jid = 0; jid < this->instance.job_count(); ++jid) {
		Expression dur_expr = this->env.create_expression();
		dur_expr += this->instance.get_job(jid).get_duration();

		//
		// Duration variable
		//
		for (const auto & edge :
		     this->instance.get_laggraph().reverse_neighbors(jid)) {
#ifdef ENABLE_ASSERTIONS
			assert(edge.s == jid);
#endif

			if (edge.max_recharge == 0) {
				continue;
			}

			// Necessary to cap at 0
			Variable drain_addition_var = this->model.add_var(
			    VarType::CONTINUOUS, 0, MIPSolver::INFTY,
			    std::string("drain_addition_") + std::to_string(edge.s) +
			        std::string("->") + std::to_string(edge.s));

			// TODO FIXME this is slow
			this->model.commit_variables();

			// TODO FIXME how is the max recharge honored here?
			Expression gap =
			    this->start_points[edge.t] - this->start_points[edge.s] - edge.lag;
			Expression extension = gap * edge.drain_factor;
			this->model.add_constraint(extension, drain_addition_var,
			                           MIPSolver::INFTY);

			dur_expr += drain_addition_var;
		}

		// TODO is an upper bounding enough here?
		// FIXME NO!
		Expression var_expr = this->duration_variables[jid] * 1;
		this->model.add_constraint(dur_expr, var_expr, MIPSolver::INFTY);

		//
		// Overduration variables
		//
		for (unsigned int t = 0; t < this->overduration_variables[jid].size();
		     ++t) {
			Expression rhs = this->env.create_expression();
			Expression lhs = this->env.create_expression();
			rhs += this->duration_variables[jid] -
			       this->instance.get_job(jid).get_duration() - t;

			// TODO FIXME I'm not sure the M is large enough?
			// TODO FIXME SOS
			lhs += this->overduration_variables[jid][t] * 2 * this->latest_deadline;

			this->model.add_constraint(rhs, lhs, MIPSolver::INFTY);
		}

		//
		// ANDing of overduration and switch-on
		//
		for (unsigned int od_length = 0;
		     od_length < this->overduration_variables[jid].size(); ++od_length) {
			for (unsigned int t = 0; t < this->time_step_bounds[jid].second; ++t) {
				this->model.add_constraint(
				    this->overduration_variables[jid][od_length] +
				        this->variables[jid][t],
				    this->overduration_and_swon_variables[jid][t][od_length] * 2,
				    MIPSolver::INFTY);
			}
		}
	}
}

template <class SolverT>
void
DTILP<SolverT>::print_profile() const
{
	std::vector<double> sums(this->instance.resource_count(), 0.0);

	for (unsigned int t = 0; t < this->latest_deadline; t++) {
		BOOST_LOG(l.d(2)) << "====> Step " << t;
		for (unsigned int r = 0; r < this->instance.resource_count(); ++r) {
			double overshoot =
			    this->model.get_variable_assignment(this->overshoot_variables[r][t]);
			// double overshoot =
			// this->overshoot_variables[r][t].get(GRB_DoubleAttr_X);
			sums[r] += overshoot;
			BOOST_LOG(l.d(2)) << "    Res " << r << " overshoot: " << overshoot;
		}
	}

	for (unsigned int r = 0; r < this->instance.resource_count(); ++r) {
		BOOST_LOG(l.d(2)) << "======> Overshoot sum for Res " << r << ": "
		                  << sums[r];
	}
}

template <class SolverT>
void
DTILP<SolverT>::warmstart_with_fixed(
    std::vector<bool> fixed_jobs, std::vector<unsigned int> start_pos,
    unsigned int time_limit,
    Maybe<double> extension_time_usage_cost_coefficient,
    Maybe<double> extension_job_usage_cost_coefficient,
    Maybe<unsigned int> extension_time_limit,
    Maybe<unsigned int> extension_job_limit)
{
	BOOST_LOG(l.i()) << "Warm-starting.";

	for (unsigned int jid = 0; jid < this->instance.job_count(); ++jid) {
		this->model.set_start(this->start_points[jid], start_pos[jid]);
		// TODO also set start-indicators?

		if (fixed_jobs[jid] && !this->job_is_fixed[jid]) {
			this->fix_job(jid, start_pos[jid]);
		} else if (!fixed_jobs[jid] && this->job_is_fixed[jid]) {
			this->unfix_job(jid);
		}
	}

	if (extension_time_usage_cost_coefficient.valid()) {
		this->model.change_objective_coefficient(
		    this->window_extension_time_var,
		    extension_time_usage_cost_coefficient.value());
	} else {
		this->model.change_objective_coefficient(this->window_extension_time_var,
		                                         0);
	}

	if (extension_job_usage_cost_coefficient.valid()) {
		this->model.change_objective_coefficient(
		    this->window_extension_job_var,
		    extension_job_usage_cost_coefficient.value());
	} else {
		this->model.change_objective_coefficient(this->window_extension_job_var, 0);
	}

	if (extension_time_limit.valid()) {
		this->model.change_constraint_ub(this->window_extension_time_constraint,
		                                 extension_time_limit.value());
	} else {
		// TODO FIXME
		// this->model.change_constraint_ub(this->window_extension_time_constraint,
		//                                 this->instance.get_window_extension_limit());
	}

	if (extension_job_limit.valid()) {
		this->model.change_constraint_ub(this->window_extension_job_constraint,
		                                 extension_job_limit.value());
	} else {
		// TODO FIXME
		// this->model.change_constraint_ub(this->window_extension_job_constraint,
		//                                this->instance.get_window_extension_job_limit());
	}

	this->solve(time_limit);
}

template <class SolverT>
void
DTILP<SolverT>::fix_job(unsigned int jid, unsigned int time)
{
	this->model.change_var_bounds(this->start_points[jid], time, time);
	// TODO also change derived variables?
}

template <class SolverT>
void
DTILP<SolverT>::unfix_job(unsigned int jid)
{
	this->model.change_var_bounds(
	    this->start_points[jid], this->time_step_bounds[jid].first,
	    this->time_step_bounds[jid].first + this->time_step_bounds[jid].second);
	// TODO also change derived variables?
}

template <class SolverT>
void
DTILP<SolverT>::run()
{
	this->prepare();
	this->base_run();
}

template <class SolverT>
std::string
DTILP<SolverT>::get_id()
{
	return "DTILP v2.3 (" + std::string(SolverT::NAME) + ")";
}

// explicit instantiation

#if defined(GUROBI_FOUND)
template class DTILP<ilpabstraction::GurobiInterface>;
#endif

#if defined(CPLEX_FOUND)
template class DTILP<ilpabstraction::CPLEXInterface>;
#endif
