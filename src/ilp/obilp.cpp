#include "obilp.hpp"

#include "../algorithms/graphalgos.hpp"             // For CriticalPathComputer
#include "../contrib/ilpabstraction/src/common.hpp" // for VariableType, Varia...
#include "../instance/instance.hpp"                 // IWYU pragma: keep
#include "../instance/job.hpp"                      // for Job
#include "gurobi_c++.h"
#include "ilp.hpp" // for ILPBase

#include <unordered_set>

class AdditionalResultStorage;
class SolverConfig;
namespace ilpabstraction {
class GurobiInterface;
}

/*
 * TODO FIXME create variables / constraints only for jobs that can potentially
 * overlap!
 */

template <class SolverT>
OBILP<SolverT>::OBILP(const Instance & instance_in,
                      AdditionalResultStorage & additional_in,
                      const SolverConfig & sconf_in)
    : ILPBase<SolverT>(instance_in, additional_in, sconf_in), l("OBILP")
{}

template <class SolverT>
std::string
OBILP<SolverT>::get_id()
{
	return "OBILP v0.2 (" + std::string(SolverT::NAME) + ")";
}

template <class SolverT>
void
OBILP<SolverT>::generate_events() noexcept
{
	this->events.clear();
	this->events.reserve(2 * this->instance.job_count());

	CriticalPathComputer cp(this->instance);
	this->earliest_starts = cp.get_forward();
	this->latest_finishs = cp.get_reverse();

	for (unsigned int jid = 0; jid < this->instance.job_count(); ++jid) {
		this->events.push_back({jid, this->earliest_starts[jid], true});
		this->events.push_back({jid, this->latest_finishs[jid], false});
	}

	std::sort(this->events.begin(), this->events.end(),
	          [](const Event & lhs, const Event & rhs) {
		          if (lhs.time != rhs.time) {
			          return lhs.time < rhs.time;
		          }
		          if (lhs.start != rhs.start) {
			          return lhs.start;
		          }

		          return lhs.jid < rhs.jid;
	          });
}

template <class SolverT>
void
OBILP<SolverT>::prepare_variables()
{
	this->generate_vars_start_points();

	this->disjunct_vars.resize(this->instance.job_count());
	this->order_vars.resize(this->instance.job_count());
	std::unordered_set<unsigned int> open_jids;

	for (const Event & ev : this->events) {
		unsigned int jid_a = ev.jid;
		const Job & job_a = this->instance.get_job(jid_a);
		if (ev.start) {
			for (unsigned int jid_b : open_jids) {
				const Job & job_b = this->instance.get_job(jid_b);
				/*
				 * After-Vars
				 */
				if (this->latest_finishs[jid_b] - job_b.get_duration() >=
				    this->earliest_starts[jid_a] + job_a.get_duration()) {
					// b can be completely after a
					this->disjunct_vars[jid_a].insert(
					    {jid_b,
					     this->model.add_var(ilpabstraction::VariableType::BINARY, 0, 1,
					                         std::to_string(jid_a) +
					                             std::string("_starts_after_") +
					                             std::to_string(jid_b))});
				}
				if (this->latest_finishs[jid_a] - job_a.get_duration() >=
				    this->earliest_starts[jid_b] + job_b.get_duration()) {
					// a can be completely after b
					this->disjunct_vars[jid_b].insert(
					    {jid_a,
					     this->model.add_var(ilpabstraction::VariableType::BINARY, 0, 1,
					                         std::to_string(jid_b) +
					                             std::string("_starts_after_") +
					                             std::to_string(jid_a))});
				}

				/*
				 * Before-Vars
				 */
				if ((this->earliest_starts[jid_a] <=
				     this->latest_finishs[jid_b] -
				         job_b.get_duration()) && // job_a not too late
				    (this->latest_finishs[jid_a] - job_a.get_duration() >=
				     this->earliest_starts[jid_b])) // job_a not too early
				{
					this->order_vars[jid_a].insert(
					    {jid_b,
					     this->model.add_var(ilpabstraction::VariableType::BINARY, 0, 1,
					                         std::to_string(jid_a) +
					                             std::string("_starts_before_") +
					                             std::to_string(jid_b))});
				}

				if ((this->earliest_starts[jid_b] <=
				     this->latest_finishs[jid_a] - job_a.get_duration()) &&
				    (this->latest_finishs[jid_b] - job_b.get_duration() >=
				     this->earliest_starts[jid_a])) {
					// b can start before (or at the same time as) a starts
					this->order_vars[jid_b].insert(
					    {jid_a,
					     this->model.add_var(ilpabstraction::VariableType::BINARY, 0, 1,
					                         std::to_string(jid_b) +
					                             std::string("_starts_before_") +
					                             std::to_string(jid_a))});
				}
			}
			open_jids.insert(jid_a);
		} else {
			open_jids.erase(jid_a);
		}
	}
}

template <class SolverT>
void
OBILP<SolverT>::prepare_after_constraints()
{
	for (unsigned int jid_a = 0; jid_a < this->instance.job_count(); ++jid_a) {
		const Job & job_a = this->instance.get_job(jid_a);

		for (auto & [jid_b, var] : this->disjunct_vars[jid_a]) {
			this->model.add_constraint(
			    SolverT::NEGATIVE_INFTY, var,
			    1 + ((this->start_points[jid_b] - this->start_points[jid_a] -
			          job_a.get_duration()) /
			         this->latest_deadline),
			    std::string("constr_") + std::to_string(jid_b) +
			        std::string("_after_") + std::to_string(jid_a));
		}

		/*
		for (unsigned int j = 0; j < this->instance.job_count(); ++j) {
		  this->model.add_constraint(
		      SolverT::NEGATIVE_INFTY, this->disjunct_vars[i][j],
		      1 + ((this->start_points[j] - this->start_points[i] -
		            job_i.get_duration()) /
		           this->latest_deadline),
		      std::string("constr_") + std::to_string(j) + std::string("_after_") +
		          std::to_string(i));
		}
		*/
	}
}

template <class SolverT>
void
OBILP<SolverT>::prepare_before_constraints()
{
	for (unsigned int jid_a = 0; jid_a < this->instance.job_count(); ++jid_a) {
		for (auto & [jid_b, var] : this->order_vars[jid_a]) {
			// 'var' is here: 'a starts before (or at the same time) as b starts',
			// i.e., must be 1 if start_points[b] >= start_points[a]-
			// TODO EPSILON?
			this->model.add_constraint(
			    (this->start_points[jid_b] - this->start_points[jid_a] + 1) /
			        this->latest_deadline,
			    var, SolverT::INFTY,
			    std::string("constr_") + std::to_string(jid_a) +
			        std::string("_before_") + std::to_string(jid_b));
		}
		/*
		for (unsigned int j = 0; j < this->instance.job_count(); ++j) {
		  this->model.add_constraint(
		      (this->start_points[j] - this->start_points[i] + 1) /
		          this->latest_deadline,
		      this->order_vars[i][j], SolverT::INFTY,
		      std::string("constr_") + std::to_string(i) + std::string("_before_") +
		          std::to_string(j));
		}
		*/
	}
}

template <class SolverT>
void
OBILP<SolverT>::prepare_start_usage_exprs()
{
	this->start_usage.resize(this->instance.resource_count());
	/*
	for (unsigned int rid = 0; rid < this->instance.resource_count(); ++rid) {
	  this->start_usage[rid].resize(
	      this->instance
	          .job_count()); // TODO make sure this works with all ILP interfaces

	  std::generate(this->start_usage[rid].begin(), this->start_usage[rid].end(),
	                [&]() { return this->env.create_expression(); });
	}
	*/
	// this->start_usage_var.resize(this->instance.resource_count());

	std::unordered_set<unsigned int> open_jids;

	for (unsigned int rid = 0; rid < this->instance.resource_count(); ++rid) {
		this->start_usage[rid].resize(this->instance.job_count());

		for (const Event & ev : this->events) {
			unsigned int jid_outer = ev.jid;

			if (ev.start) {
				const Job & job_outer = this->instance.get_job(jid_outer);

				// Initialize this job_a's expression
				this->start_usage[rid][jid_outer] = this->env.create_expression();
				auto & expr_outer = this->start_usage[rid][jid_outer];
				expr_outer += job_outer.get_resource_usage(rid);

				for (unsigned int jid_inner : open_jids) {
					const Job & job_inner = this->instance.get_job(jid_inner);
					/* Anonymous helper lambda to construct the part of the expression of
					 * job_a's start usage contributed by job_b.
					 *
					 * The logic is as follows:
					 *
					 * - if this->order_vars[job_b][job_a] *and*
					 *   this->disjunct_vars[job_b][job_a] exist, everything is as usual.
					 * - if only this->order_vars[job_b][job_a] exists, that means that
					 *   the situation that a starts after b has finished can never arise.
					 *   Thus, we only have to look for this->order_vars[job_b][job_a].
					 * - if only this->disjunct_vars[job_b][job_a] exists, that means that
					 *   either job_b *always* starts before job_a, or *never*. We check
					 * and act accordingly.
					 * - if neither exist, same as case 3.
					 *
					 * // TODO der check ist bisher nur einseitig oben
					 */
					auto adder = [&](const Job & job_a, unsigned int jid_a,
					                 const Job & job_b, unsigned int jid_b,
					                 decltype(expr_outer) & expr_a) {
						if ((this->disjunct_vars[jid_b].find(jid_a) !=
						     this->disjunct_vars[jid_b].end()) &&
						    (this->order_vars[jid_b].find(jid_a) !=
						     this->order_vars[jid_b].end())) {
							expr_a += (this->order_vars[jid_b].at(jid_a) -
							           this->disjunct_vars[jid_b].at(jid_a)) *
							          job_b.get_resource_usage(rid);
						} else if (this->order_vars[jid_b].find(jid_a) !=
						           this->order_vars[jid_b].end()) {
							expr_a += (this->order_vars[jid_b].at(jid_a) *
							           job_b.get_resource_usage(rid));
						} else if (this->disjunct_vars[jid_b].find(jid_a) !=
						           this->disjunct_vars[jid_b].end()) {
							if (this->latest_finishs[jid_b] - job_b.get_duration() <
							    this->earliest_starts[jid_a]) {
								// job_b always starts before job_a.
								expr_a += ((1 - this->disjunct_vars[jid_b].at(jid_a)) *
								           job_b.get_resource_usage(rid));
							} else {
								// job_b *never* starts before job_a.
								assert(this->latest_finishs[jid_a] - job_a.get_duration() <
								       this->earliest_starts[jid_b]);
							}
						} else {
							// Nothing is there. If job_b always starts before job_a, add the
							// usage, otherwise don't
							if (this->latest_finishs[jid_b] - job_b.get_duration() <
							    this->earliest_starts[jid_a]) {
								// job_b always starts before job_a.
								expr_a += job_b.get_resource_usage(rid);
							} else {
								// job_b *never* starts before job_a.
								assert(this->latest_finishs[jid_a] - job_a.get_duration() <
								       this->earliest_starts[jid_b]);
							}
						}
					};

					// Add job_b to job_a's start
					adder(job_outer, jid_outer, job_inner, jid_inner, expr_outer);

					// Add job_a to job_b's start
					auto & expr_inner = this->start_usage[rid][jid_inner];
					adder(job_inner, jid_inner, job_outer, jid_outer, expr_inner);
				}

				open_jids.insert(jid_outer);

			} else {
				open_jids.erase(jid_outer);
			}
		}
	}

	for (unsigned int rid = 0; rid < this->instance.resource_count(); ++rid) {
		for (unsigned int jid = 0; jid < this->instance.job_count(); ++jid) {
			auto & expr = this->start_usage[rid].at(jid);
			this->usage_constraints.push_back(this->model.add_constraint(
			    expr, this->max_usage_variables[rid], SolverT::INFTY,
			    std::string("usage_res_") + std::to_string(rid) +
			        std::string("_start_of_") + std::to_string(jid)));
		}
	}

	/*
	for (unsigned int rid = 0; rid < this->instance.resource_count(); ++rid) {
	  for (unsigned int jid = 0; jid < this->instance.job_count(); ++jid) {
	    const Job & job = this->instance.get_job(jid);

	    auto expr = this->env.create_expression();
	    expr += job.get_resource_usage(rid);

	    for (unsigned int i = 0; i < this->instance.job_count(); ++i) {
	      if (i == jid) {
	        continue;
	      }

	      const Job & i_job = this->instance.get_job(i);

	      expr += (this->order_vars[i][jid] - this->disjunct_vars[i][jid]) *
	              i_job.get_resource_usage(rid);
	    }

	    this->start_usage[rid].push_back(expr);*/

	/* TODO DEBUG */
	/*
	this->start_usage_var[rid].push_back(
	                this->model.add_var(ilpabstraction::VariableType::CONTINUOUS,
	0, SolverT::INFTY, std::string("usage_res")+ std::to_string(rid) +
	                  std::string("_start_of_") + std::to_string(jid)));
	this->model.commit_variables();
	auto & var = this->start_usage_var[rid][this->start_usage_var[rid].size()
	- 1]; this->model.add_constraint(expr, var, SolverT::INFTY);
	this->model.add_constraint(var, this->max_usage_variables[rid],
	SolverT::INFTY);
	*/
	/*
	    this->model.add_constraint(
	        expr, this->max_usage_variables[rid], SolverT::INFTY,
	        std::string("usage_res_") + std::to_string(rid) +
	            std::string("_start_of_") + std::to_string(jid));
	  }
	}
	*/
}

template <class SolverT>
void
OBILP<SolverT>::prepare()
{
	this->prepare_pre();
	this->generate_events();

	BOOST_LOG(l.d()) << "Preparing variables...";
	this->prepare_variables();
	BOOST_LOG(l.d()) << "Preparing after-constraints...";
	this->prepare_after_constraints();
	BOOST_LOG(l.d()) << "Preparing before-constraints...";
	this->prepare_before_constraints();
	BOOST_LOG(l.d()) << "Preparing usage expressions...";
	this->prepare_start_usage_exprs();

	this->prepare_post();
}

template <class SolverT>
void
OBILP<SolverT>::run()
{
	this->prepare();
	this->base_run();

	// TODO DEBUG REMOVE
	/*
	for (auto & constraint : this->usage_constraints) {
	  // TODO this is gurobi-internal!
	  auto & lower = constraint.first;
	  double slack = lower.get(GRB_DoubleAttr_Slack);
	  std::string name = lower.get(GRB_StringAttr_ConstrName);

	  //		std::cout << "Slack: " << slack << "\n";
	  if (slack < 0.1) {
	    // This is tight
	    std::cout << "Constraint is tight: " << name << "\n";
	  }

	  std::string should_be_tight = "usage_res_0_start_of_97_lower";
	  if (should_be_tight.compare(name) == 0) {
	    std::cout << "usage_res_0_start_of_97_lower has slack: " << slack << "\n";
	  }
	}

	int point = (unsigned int)std::round(
	    this->model.get_variable_assignment(this->start_points[71]));
	std::cout << "Jobs overlapping 71's start: \n";
	double usage_sum = 0;
	for (unsigned int jid = 0; jid < this->instance.job_count(); ++jid) {
	  int start = (unsigned int)std::round(
	      this->model.get_variable_assignment(this->start_points[jid]));

	  int end = start + this->instance.get_job(jid).get_duration();

	  if ((start <= point) && (end > point)) {
	    std::cout << " - " << jid << " has :";
	    if (this->order_vars[jid].find(71) != this->order_vars[jid].end()) {
	      std::cout << " before-var ";
	      double val =
	          this->model.get_variable_assignment(this->order_vars[jid].at(71));
	      if (val > 0.5) {
	        std::cout << "(true) ";
	      } else {
	        std::cout << "(false) ";
	      }
	    }
	    if (this->disjunct_vars[jid].find(71) != this->disjunct_vars[jid].end()) {
	      std::cout << " after-var ";
	      double val =
	          this->model.get_variable_assignment(this->disjunct_vars[jid].at(71));
	      if (val > 0.5) {
	        std::cout << "(true) ";
	      } else {
	        std::cout << "(false) ";
	      }
	    }
	    std::cout << "\n";

	    usage_sum += this->instance.get_job(jid).get_resource_usage(0);
	  }
	}
	std::cout << "(" << usage_sum << ") \n";
	*/
}

// explicit instantiation

#if defined(GUROBI_FOUND)
template class OBILP<ilpabstraction::GurobiInterface>;
#endif

#if defined(CPLEX_FOUND)
template class OBILP<ilpabstraction::CPLEXInterface>;
#endif
