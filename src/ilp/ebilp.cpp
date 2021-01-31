//
// Created by lukas on 10.07.18.
//

#include "ebilp.hpp"

#include "../instance/instance.hpp" // IWYU pragma: keep
#include "../instance/laggraph.hpp" // IWYU pragma: keep
#include "../util/solverconfig.hpp" // IWYU pragma: keep
#include "algorithms/graphalgos.hpp"
#include "contrib/ilpabstraction/src/common.hpp" // for VariableType, Varia...
#include "datastructures/maybe.hpp"              // for Maybe
#include "ilp/ilp.hpp"                           // for ILPBase
#include "instance/job.hpp"                      // for Job

#include <boost/log/core/record.hpp>            // for record
#include <boost/log/sources/record_ostream.hpp> // for BOOST_LOG
#include <iostream>                             // for operator<<, cout
#include <math.h>                               // for round
#include <stddef.h>                             // for size_t
#include <unordered_map>

class AdditionalResultStorage;
class Instance;
namespace ilpabstraction {
class GurobiInterface;
}

template <class SolverT>
EBILP<SolverT>::EBILP(const Instance & instance_in,
                      AdditionalResultStorage & additional_in,
                      const SolverConfig & sconf_in)
    : ILPBase<SolverT>(instance_in, additional_in, sconf_in), l("EBILP"),
      start_point_mode(false), enforce_end_after_start_via_sum(true)
{
	if (this->sconf.has_config("use_start_points")) {
		this->start_point_mode = (bool)this->sconf["use_start_points"];
	}
	if (this->sconf.has_config("enforce_order_by_sum")) {
		this->enforce_end_after_start_via_sum =
		    (bool)this->sconf["enforce_order_by_sum"];
	}
}

template <class SolverT>
std::string
EBILP<SolverT>::get_id()
{
	return "EBILP v0.1 (" + std::string(SolverT::NAME) + ")";
}

template <class SolverT>
void
EBILP<SolverT>::compute_skip_numbers() noexcept
{
	NecessaryOrderComputer noc(this->instance);
	auto predecessors = noc.get_predecessor_count();
	auto successors = noc.get_successor_count();

	this->max_deadline = 0;

	this->skip_numbers.resize(this->instance.job_count());
	this->event_counts.resize(this->instance.job_count());
	for (unsigned int jid = 0; jid < this->instance.job_count(); ++jid) {
		// Every predecessor must have start and end strictly before us,
		// vice versa for successors.
		this->skip_numbers[jid] = {2 * predecessors[jid], 2 * successors[jid]};
		this->event_counts[jid] = 2 * this->instance.job_count() -
		                          2 * predecessors[jid] - 2 * successors[jid];
		const auto & job = this->instance.get_job(jid);
		this->max_deadline = std::max(this->max_deadline, job.get_deadline());
	}
}

template <class SolverT>
void
EBILP<SolverT>::prepare_variables()
{
	if (this->start_point_mode) {
		this->generate_vars_start_points();
	}

	size_t event_count = 2 * this->instance.job_count();

	this->job_start_events.resize(this->instance.job_count());
	this->job_end_events.resize(this->instance.job_count());
	for (unsigned int jid = 0; jid < this->instance.job_count(); ++jid) {
		size_t necessary_events = this->event_counts[jid];
		for (size_t eid = 0; eid < necessary_events; ++eid) {
			assert(this->skip_numbers[jid].first + eid < event_count);
			this->job_start_events[jid].push_back(this->model.add_var(
			    ilpabstraction::VariableType::BINARY, 0, 1,
			    std::to_string(jid) + std::string("_starts_at_") +
			        std::to_string(eid + this->skip_numbers[jid].first)));
			this->job_end_events[jid].push_back(this->model.add_var(
			    ilpabstraction::VariableType::BINARY, 0, 1,
			    std::to_string(jid) + std::string("_ends_at_") +
			        std::to_string(eid + this->skip_numbers[jid].first)));
		}
	}

	for (size_t eid = 0; eid < event_count; ++eid) {
		this->event_times.push_back(this->model.add_var(
		    ilpabstraction::VariableType::CONTINUOUS, 0, SolverT::INFTY,
		    std::string("event_") + std::to_string(eid) + "_time"));
	}

	/*
	        for (unsigned int rid = 0 ; rid < this->instance.resource_count() ;
	   ++rid) { this->usages_after_event.emplace_back(); for (unsigned int eid =
	   0; eid < event_count; ++eid) { this->usages_after_event[rid].push_back(
	                                                        this->model.add_var(ilpabstraction::VariableType::CONTINUOUS,
	   0, SolverT::INFTY, std::string("usage_rid_") + std::to_string(rid)
	                                                                                                                + std::string("_after_") + std::to_string(eid)));
	                }
	        }
	 */
}

template <class SolverT>
void
EBILP<SolverT>::prepare_time_constraints()
{
	/*
	for (unsigned int jid = 0 ; jid < this->instance.job_count() ; ++jid) {
	        const Job & job = this->instance.get_job(jid);

	        this->model.add_constraint(job.get_duration(),
	                                  this->end_points[jid] -
	this->start_points[jid], SolverT::INFTY,
	std::string("event_separation_for_jid_")
	                                  + std::to_string(jid));
	}
	 */

	for (unsigned int jid = 0; jid < this->instance.job_count(); ++jid) {
		const Job & job = this->instance.get_job(jid);

		if (this->enforce_end_after_start_via_sum) {
			Expression start_sum;
			Expression end_sum;
			for (unsigned int ev = 0; ev < this->event_counts[jid]; ++ev) {
				start_sum += (ev * this->job_start_events[jid][ev]);
				end_sum += (ev * this->job_end_events[jid][ev]);
			}

			this->model.add_constraint(start_sum + 1, end_sum, SolverT::INFTY,
			                           "enforce_start_end_order_" +
			                               std::to_string(jid));
		}

		for (unsigned int start_ev = 0; start_ev < this->event_counts[jid];
		     ++start_ev) {
			for (unsigned int end_ev = 0; end_ev < this->event_counts[jid];
			     ++end_ev) {
				if (end_ev <= start_ev) {
					// This may never happen
					// If we didn't use the sum trick above, we have to manually forbid
					// this
					if (!this->enforce_end_after_start_via_sum) {
						this->model.add_constraint(
						    0,
						    this->job_start_events[jid][start_ev] +
						        this->job_end_events[jid][end_ev],
						    1,
						    "forbid_jid_" + std::to_string(jid) + std::string("_end_at_") +
						        std::to_string(end_ev + this->skip_numbers[jid].first) +
						        std::string("_before_") +
						        std::to_string(start_ev + this->skip_numbers[jid].first));
					}
				} else {
					// If both are one, the duration between both events must be long
					// enough
					this->model.add_constraint(
					    ((this->job_start_events[jid][start_ev] +
					      this->job_end_events[jid][end_ev] - 1) *
					     job.get_duration()),
					    this->event_times[end_ev + this->skip_numbers[jid].first] -
					        this->event_times[start_ev + this->skip_numbers[jid].first],
					    SolverT::INFTY,
					    std::string("duration_") + std::to_string(jid) +
					        std::string("_between_") +
					        std::to_string(start_ev + this->skip_numbers[jid].first) +
					        std::string("_and_") +
					        std::to_string(end_ev + this->skip_numbers[jid].first));
				}
			}
		}
	}
}

template <class SolverT>
void
EBILP<SolverT>::prepare_start_points_constraints()
{
	for (unsigned int jid = 0; jid < this->instance.job_count(); ++jid) {
		for (unsigned int eid = 0; eid < this->event_counts[jid]; ++eid) {
			this->model.add_constraint(
			    this->event_times[eid + this->skip_numbers[jid].first] -
			        ((1 - this->job_start_events[jid][eid]) * this->latest_deadline),
			    this->start_points[jid],
			    this->event_times[eid + this->skip_numbers[jid].first] +
			        ((1 - this->job_start_events[jid][eid]) * this->latest_deadline));
		}
	}
}

template <class SolverT>
void
EBILP<SolverT>::prepare_dependency_constraints()
{
	for (auto & edge : this->instance.get_laggraph().edges()) {
		const Job & s_job = this->instance.get_job(edge.s);
		const Job & t_job = this->instance.get_job(edge.t);

		for (unsigned int start_ev = 0; start_ev < this->event_counts[edge.s];
		     ++start_ev) {
			for (unsigned int end_ev = 0; end_ev < this->event_counts[edge.t];
			     ++end_ev) {

				// this->latest_deadline is being used as big-M here
				this->model.add_constraint(
				    (((this->job_start_events[s_job.get_jid()][start_ev] +
				       this->job_start_events[t_job.get_jid()][end_ev] - 1) *
				      (edge.lag + static_cast<int>(this->latest_deadline))) -
				     static_cast<int>(this->latest_deadline)),
				    this->event_times[end_ev + this->skip_numbers[edge.t].first] -
				        this->event_times[start_ev + this->skip_numbers[edge.s].first],
				    SolverT::INFTY,
				    std::string("dependency_from_") + std::to_string(s_job.get_jid()) +
				        std::string("_to_") + std::to_string(t_job.get_jid()) +
				        std::string("_between_") +
				        std::to_string(start_ev + this->skip_numbers[edge.s].first) +
				        std::string("_and_") +
				        std::to_string(end_ev + this->skip_numbers[edge.t].first));
			}
		}
	}
}

template <class SolverT>
void
EBILP<SolverT>::create_solution()
{
	this->computed_solution_start_times = std::vector<Maybe<unsigned int>>(
	    this->instance.job_count(), Maybe<unsigned int>());

	if (!this->model.has_feasible()) {
		BOOST_LOG(l.w()) << "ILP has found no feasible solution.";
		return;
	}

	for (unsigned int jid = 0; jid < this->instance.job_count(); ++jid) {
		for (unsigned int eid = 0; eid < this->event_counts[jid]; ++eid) {
			if (this->model.get_variable_assignment(
			        this->job_start_events[jid][eid]) >= 0.8) {
				/*				std::cout << "Job " << jid << " starts at event "
				          << eid + this->skip_numbers[jid].first << " at time "
				          << this->model.get_variable_assignment(
				                 this->event_times[eid + this->skip_numbers[jid].first])
				                 << "\n"; */
				this->computed_solution_start_times[jid] =
				    (unsigned int)std::round(this->model.get_variable_assignment(
				        this->event_times[eid + this->skip_numbers[jid].first]));
			}
		}
	}
}

template <class SolverT>
void
EBILP<SolverT>::prepare_release_deadline_constraints()
{
	for (unsigned int jid = 0; jid < this->instance.job_count(); ++jid) {
		const Job & job = this->instance.get_job(jid);

		for (unsigned int eid = 0; eid < this->event_counts[jid]; ++eid) {
			this->model.add_constraint(
			    SolverT::NEGATIVE_INFTY,
			    this->event_times[eid + this->skip_numbers[jid].first] -
			        ((1 - this->job_end_events[jid][eid]) * this->latest_deadline),
			    job.get_deadline(),
			    std::string("release_") + std::to_string(jid) + std::string("_at_") +
			        std::to_string(eid + this->skip_numbers[jid].first));
			this->model.add_constraint(
			    job.get_release(),
			    this->event_times[eid + this->skip_numbers[jid].first] +
			        ((1 - this->job_start_events[jid][eid]) * this->latest_deadline),
			    SolverT::INFTY,
			    std::string("deadline_") + std::to_string(jid) + std::string("_at_") +
			        std::to_string(eid + this->skip_numbers[jid].first));
		}
	}
}

template <class SolverT>
void
EBILP<SolverT>::prepare_duration_constraint()
{
	for (unsigned int jid = 0; jid < this->instance.job_count(); ++jid) {
		const Job & job = this->instance.get_job(jid);

		this->model.add_constraint(
		    job.get_duration(), this->duration_variables[jid], job.get_duration(),
		    std::string("job_") + std::to_string(jid) + std::string("_duration"));
	}
}

template <class SolverT>
void
EBILP<SolverT>::prepare_event_constraints()
{
	size_t event_count = 2 * this->instance.job_count();

	for (unsigned int eid = 0; eid < event_count - 1; ++eid) {
		this->model.add_constraint(this->event_times[eid],
		                           this->event_times[eid + 1], SolverT::INFTY);
	}
}

template <class SolverT>
void
EBILP<SolverT>::prepare_usage_expressions()
{
	size_t event_count = 2 * this->instance.job_count();
	/*
	        for (unsigned int eid = 0 ; eid < event_count ; ++eid) {
	                for (unsigned int rid = 0 ; rid <
	   this->instance.resource_count() ; ++rid) { Expression usage =
	   this->env.create_expression(); if (eid > 0) { usage +=
	   this->usages_after_event[rid][eid - 1];
	                        }

	                        for (unsigned int jid = 0 ; jid <
	   this->instance.job_count() ; ++jid) { const Job & job =
	   this->instance.get_job(jid); usage += this->job_start_events[jid][eid]
	                                                                *
	   job.get_resource_usage(rid); usage -= this->job_end_events[jid][eid]
	                                                                *
	   job.get_resource_usage(rid);
	                        }

	                        this->model.add_constraint(usage,
	   this->usages_after_event[rid][eid], SolverT::INFTY);
	                        this->model.add_constraint(usage,
	   this->max_usage_variables[rid], SolverT::INFTY);
	                }
	        }
	*/

	this->event_usages.resize(this->instance.resource_count(),
	                          std::vector<Expression>(event_count));

	for (unsigned int eid = 0; eid < event_count; ++eid) {
		for (unsigned int rid = 0; rid < this->instance.resource_count(); ++rid) {
			Expression last_event_expression;
			if (eid > 0) {
				last_event_expression = this->event_usages[rid][eid - 1];
			} else {
				last_event_expression = this->env.create_expression();
			}

			this->event_usages[rid][eid] =
			    this->env.create_expression() + last_event_expression;
			for (unsigned int jid = 0; jid < this->instance.job_count(); ++jid) {
				const Job & job = this->instance.get_job(jid);
				if ((eid < this->skip_numbers[jid].first) ||
				    (eid >= event_count - this->skip_numbers[jid].second)) {
					continue;
				}

				this->event_usages[rid][eid] +=
				    this->job_start_events[jid][eid - this->skip_numbers[jid].first] *
				    job.get_resource_usage(rid);
				this->event_usages[rid][eid] -=
				    this->job_end_events[jid][eid - this->skip_numbers[jid].first] *
				    job.get_resource_usage(rid);
			}

			this->model.add_constraint(
			    this->event_usages[rid][eid], this->max_usage_variables[rid],
			    SolverT::INFTY,
			    std::string("usage_res_") + std::to_string(rid) +
			        std::string("_after_ev_") + std::to_string(eid));
		}
	}
}

template <class SolverT>
void
EBILP<SolverT>::prepare_job_event_constraints()
{

	/*
	std::vector<Expression> event_expressions(event_count);
	for (size_t i = 0; i < event_count; ++i) {
	  event_expressions[i] = this->env.create_expression();
	}
	*/

	for (unsigned int jid = 0; jid < this->instance.job_count(); ++jid) {
		Expression start_expr = this->env.create_expression();
		Expression end_expr = this->env.create_expression();

		for (unsigned int eid = 0; eid < this->event_counts[jid]; ++eid) {
			start_expr += this->job_start_events[jid][eid];
			end_expr += this->job_end_events[jid][eid];
			/*
			event_expressions[eid] += this->job_start_events[jid][eid];
			event_expressions[eid] += this->job_end_events[jid][eid];
			*/
		}

		this->model.add_constraint(1, start_expr, 1,
		                           std::string("start_") + std::to_string(jid) +
		                               std::string("_once"));
		this->model.add_constraint(1, end_expr, 1,
		                           std::string("end_") + std::to_string(jid) +
		                               std::string("_once"));
	}

	/*
	for (size_t i = 0; i < event_count; ++i) {
	        this->model.add_constraint(1, event_expressions[i], 1,
	std::string("event_unique_") + std::to_string(i));
	}
	 */
}

template <class SolverT>
void
EBILP<SolverT>::prepare()
{
	this->prepare_pre();

	BOOST_LOG(l.d()) << "Preparing variables...";
	this->compute_skip_numbers();
	this->prepare_variables();
	BOOST_LOG(l.d()) << "Preparing duration constraints...";
	this->prepare_duration_constraint();
	BOOST_LOG(l.d()) << "Preparing event order constraints...";
	this->prepare_event_constraints();
	BOOST_LOG(l.d()) << "Preparing job / event linking constraints...";
	this->prepare_job_event_constraints();
	BOOST_LOG(l.d()) << "Preparing time between event constraints...";
	this->prepare_time_constraints();
	BOOST_LOG(l.d()) << "Preparing usage constraints...";
	this->prepare_usage_expressions();

	if (this->start_point_mode) {
		BOOST_LOG(l.d()) << "Preparing start time constraints...";
		this->prepare_start_points_constraints();
	} else {
		BOOST_LOG(l.d()) << "Preparing release / deadline constraints...";
		this->prepare_release_deadline_constraints();
		BOOST_LOG(l.d()) << "Preparing dependency constraints...";
		this->prepare_dependency_constraints();
	}

	this->prepare_post();
}

template <class SolverT>
void
EBILP<SolverT>::run()
{
	this->prepare();
	this->base_run();

	if (!this->start_point_mode) {
		this->create_solution();
	}
}

// explicit instantiation

#if defined(GUROBI_FOUND)
template class EBILP<ilpabstraction::GurobiInterface>;
#endif

#if defined(CPLEX_FOUND)
template class EBILP<ilpabstraction::CPLEXInterface>;
#endif
