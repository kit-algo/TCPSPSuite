#include "fbilp.hpp"

#include "../contrib/ilpabstraction/src/common.hpp"
#include "../instance/instance.hpp"
#include "../instance/job.hpp"
#include "../src/util/solverconfig.hpp"

#include <cstddef>

template <class SolverT>
FBILP<SolverT>::FBILP(const Instance & instance_in,
                      AdditionalResultStorage & additional_in,
                      const SolverConfig & sconf_in)
    : ILPBase<SolverT>(instance_in, additional_in, sconf_in), l("FBILP"),
      proxy_flow(sconf_in.has_config("proxy_flow") &&
                 sconf_in.as_bool("proxy_flow"))

{}

template <class SolverT>
void
FBILP<SolverT>::run()
{
	BOOST_LOG(l.d()) << "Running FBILP.";
	if (this->proxy_flow) {
		BOOST_LOG(l.i()) << "Flow-proxying enabled.";
	}
	this->prepare();
	this->base_run();
}

template <class SolverT>
void
FBILP<SolverT>::prepare()
{
	this->prepare_pre();

	BOOST_LOG(l.d()) << "Preparing variables.";
	this->prepare_variables();
	BOOST_LOG(l.d()) << "Preparing sequence constraints";
	this->prepare_sequence_constraints();
	BOOST_LOG(l.d()) << "Preparing flow constraints";
	this->prepare_flow_constraints();
	BOOST_LOG(l.d()) << "Preparing objective";
	this->prepare_objective();

	this->prepare_post();
}

template <class SolverT>
void
FBILP<SolverT>::prepare_variables()
{
	if (this->proxy_flow) {
		this->proxies.resize(this->instance.job_count());
	}

	// We use the start-variables from the base ILP
	this->generate_vars_start_points();

	std::vector<const Job *> by_earliest_finish;
	std::vector<const Job *> by_latest_start;
	std::vector<const Job *> by_release;
	std::vector<const Job *> by_deadline;

	for (const auto & job : this->instance.get_jobs()) {
		by_earliest_finish.push_back(&job);
		by_latest_start.push_back(&job);
		if (this->proxy_flow) {
			by_release.push_back(&job);
			by_deadline.push_back(&job);
		}
	}

	std::sort(by_earliest_finish.begin(), by_earliest_finish.end(),
	          [](const Job * lhs, const Job * rhs) {
		          return lhs->get_release() + lhs->get_duration() <
		                 rhs->get_release() + rhs->get_duration();
	          });
	std::sort(by_latest_start.begin(), by_latest_start.end(),
	          [](const Job * lhs, const Job * rhs) {
		          return lhs->get_deadline() - lhs->get_duration() <
		                 rhs->get_deadline() - rhs->get_duration();
	          });
	if (this->proxy_flow) {
		std::sort(by_deadline.begin(), by_deadline.end(),
		          [](const Job * lhs, const Job * rhs) {
			          return lhs->get_deadline() < rhs->get_deadline();
		          });
		std::sort(by_release.begin(), by_release.end(),
		          [](const Job * lhs, const Job * rhs) {
			          return lhs->get_release() < rhs->get_release();
		          });
	}

	/*
	 * Compute proxies if necessary. The proxy for job j is the job
	 * with the earliest deadline among the jobs with a release past j's deadline.
	 */
	// TODO this could be sped up with an interval map. Is it worth it?
	if (this->proxy_flow) {
		size_t earliest_possible_proxy = 0;
		for (const auto & source_job : by_deadline) {
			while ((earliest_possible_proxy < by_release.size()) &&
			       (by_release[earliest_possible_proxy]->get_release() <
			        source_job->get_deadline())) {
				earliest_possible_proxy++;
			}

			for (auto it = by_release.begin() +
			               static_cast<ptrdiff_t>(earliest_possible_proxy);
			     it != by_release.end(); ++it) {
				if ((!this->proxies[source_job->get_jid()].has_value()) ||
				    (this->proxies[source_job->get_jid()].value()->get_deadline() >
				     (*it)->get_deadline())) {
					this->proxies[source_job->get_jid()] = *it;
				}
			}
		}
	}

	/*
	 * Create sequence variables
	 */
	auto to_it_start = by_latest_start.begin();

	this->sequence_vars.resize(this->instance.job_count());
	for (const auto & job_from : by_earliest_finish) {
		while ((to_it_start != by_latest_start.end()) &&
		       ((*to_it_start)->get_deadline() - (*to_it_start)->get_duration() <
		        job_from->get_release() + job_from->get_duration())) {
			to_it_start++;
		}

		auto to_it = to_it_start;
		while (to_it != by_latest_start.end()) {
			if (job_from->get_jid() == (*to_it)->get_jid()) {
				to_it++;
				continue;
			}

			// Check if "to" job must always start after "from" job finished. Don't
			// create a sequence variable in this case.
			if (!((*to_it)->get_release() >= job_from->get_deadline())) {
				// Jobs can actually overlap. Create the variable.
				this->sequence_vars[job_from->get_jid()].emplace(
				    (*to_it)->get_jid(),
				    this->model.add_var(ilpabstraction::VariableType::BINARY, 0, 1,
				                        std::to_string(job_from->get_jid()) +
				                            std::string("_before_") +
				                            std::to_string((*to_it)->get_jid())));
			}
			to_it++;
		}
	}

	/*
	 * Create Flow Variables
	 */
	this->flow_vars.resize(this->instance.resource_count());
	for (unsigned int r = 0; r < this->instance.resource_count(); ++r) {
		this->flow_vars[r].resize(this->instance.job_count());

		to_it_start = by_latest_start.begin();

		for (const auto & job_from : by_earliest_finish) {
			while ((to_it_start != by_latest_start.end()) &&
			       ((*to_it_start)->get_deadline() - (*to_it_start)->get_duration() <
			        job_from->get_release() + job_from->get_duration())) {
				to_it_start++;
			}

			auto to_it = to_it_start;
			while (to_it != by_latest_start.end()) {
				if (job_from->get_jid() == (*to_it)->get_jid()) {
					to_it++;
					continue;
				}

				// The flow from job_from to *to_it can be proxied via proxies[job_from]
				// if *to_it's release is after the proxy's deadline.
				if ((this->proxy_flow) &&
				    (this->proxies[job_from->get_jid()].has_value()) &&
				    ((*to_it)->get_release() >=
				     (*this->proxies[job_from->get_jid()])->get_deadline())) {
					// Can be proxied - don't create a flow variable.
				} else {
					this->flow_vars[r][job_from->get_jid()].emplace(
					    (*to_it)->get_jid(),
					    this->model.add_var(
					        ilpabstraction::VariableType::CONTINUOUS, 0, MIPSolver::INFTY,
					        std::string("flow_") + std::to_string(job_from->get_jid()) +
					            std::string("_to_") +
					            std::to_string((*to_it)->get_jid())));
				}
				to_it++;
			}
		}
	}

	/*
	 * Create inflow variables
	 */
	this->inflow_vars.resize(this->instance.resource_count());
	for (unsigned int r = 0; r < this->instance.resource_count(); ++r) {
		for (unsigned int jid = 0; jid < this->instance.job_count(); ++jid) {
			this->inflow_vars[r].push_back(this->model.add_var(
			    ilpabstraction::VariableType::CONTINUOUS, 0, MIPSolver::INFTY,
			    std::string("inflow_") + std::to_string(jid)));
		}
	}
}

template <class SolverT>
void
FBILP<SolverT>::prepare_sequence_constraints()
{
	// we skip (12) and (13) - this would be cubic. The MIP automatically enforces
	// this

	for (const auto & from_job : this->instance.get_jobs()) {
		unsigned int from_jid = from_job.get_jid();

		for (const auto & [to_jid, var] : this->sequence_vars[from_jid]) {
			// Compute the required distance from start_points[from_jid] to
			// start_points[to_jid]
			Expression required_dist = this->env.create_expression();
			// deactivate if var is 0
			required_dist +=
			    (1 - var) * (-1 * static_cast<int>(this->latest_deadline));
			required_dist += var * from_job.get_duration();
			Expression effective_dist = this->env.create_expression();
			effective_dist +=
			    this->start_points[to_jid] - this->start_points[from_jid];

			// This is (14)
			this->model.add_constraint(required_dist, effective_dist,
			                           MIPSolver::INFTY);
		}
	}
}

template <class SolverT>
void
FBILP<SolverT>::prepare_flow_constraints()
{

	for (unsigned int rid = 0; rid < this->instance.resource_count(); ++rid) {
		double max_total_flow = 0;
		if (this->proxy_flow) {
			for (const auto & job : this->instance.get_jobs()) {
				max_total_flow += job.get_resource_usage(rid);
			}
		}

		// Inflow Expressions. We build them while building the outflow constraints.
		std::vector<Expression> inflow_exprs(this->instance.job_count());
		std::generate(inflow_exprs.begin(), inflow_exprs.end(),
		              [&]() { return this->env.create_expression(); });
		std::vector<Expression> outflow_exprs(this->instance.job_count());

		// Outflow constraints
		for (const auto & job : this->instance.get_jobs()) {
			unsigned int jid = job.get_jid();

			// Collect total outflow from jid in this expression
			Expression outflow = this->env.create_expression();
			for (const auto & [in_jid, flow_var] : this->flow_vars[rid][jid]) {
				// flow from "job" -> "in_jid"
				outflow += flow_var;

				// (15), basically - the other upper limit is enforced by (17)
				if (this->instance.get_job(in_jid).get_release() >=
				    job.get_deadline()) {
					// Jobs are always in the right sequence. We don't have a sequence
					// variable.

					// Nothing to do here. Outflow amount is automatically limited by the
					// amout of flow into "job".
				} else {

					if (!this->proxy_flow) {
						// job can only pass on its own usage. This is a tighter bound than
						// in the proxy_flow case below
						this->model.add_constraint(MIPSolver::NEGATIVE_INFTY, flow_var,
						                           this->sequence_vars[jid][in_jid] *
						                               job.get_resource_usage(rid));
					} else {
						// job can also pass on proxied flow. Theoretically, it can pass all
						// flow.
						this->model.add_constraint(MIPSolver::NEGATIVE_INFTY, flow_var,
						                           this->sequence_vars[jid][in_jid] *
						                               max_total_flow);
					}
				}

				inflow_exprs[in_jid] += flow_var;
			}

			if (this->proxy_flow) {
				outflow_exprs[jid] = outflow;
			}

			// (16)
			if (!this->proxy_flow) {
				this->model.add_constraint(MIPSolver::NEGATIVE_INFTY, outflow,
				                           job.get_resource_usage(rid));
			}
		}

		// Inflow constraints
		for (const auto & job : this->instance.get_jobs()) {
			inflow_exprs[job.get_jid()] += this->inflow_vars[rid][job.get_jid()];

			// TODO set upper limit if !proxy_flow?
			// (17)
			this->model.add_constraint(job.get_resource_usage(rid),
			                           inflow_exprs[job.get_jid()], MIPSolver::INFTY);
		}

		// Limit total outflow in the case of proxy_flow. If !proxy_flow, (16) above
		// does the job
		if (this->proxy_flow) {
			for (const auto & job : this->instance.get_jobs()) {
				this->model.add_constraint(MIPSolver::NEGATIVE_INFTY,
				                           outflow_exprs[job.get_jid()],
				                           inflow_exprs[job.get_jid()]);
			}
		}
	}
}

template <class SolverT>
void
FBILP<SolverT>::prepare_objective()
{
	for (unsigned int rid = 0; rid < this->instance.resource_count(); ++rid) {
		Expression usage = this->env.create_expression();

		for (unsigned int jid = 0; jid < this->instance.job_count(); ++jid) {
			usage += this->inflow_vars[rid][jid];
		}

		this->model.add_constraint(usage, this->max_usage_variables[rid],
		                           MIPSolver::INFTY);
	}
}

template <class SolverT>
std::string
FBILP<SolverT>::get_id()
{
	return "FBILP v0.1 (" + std::string(SolverT::NAME) + ")";
}

template <class SolverT>
bool
FBILP<SolverT>::dbg_verify_variables()
{
	if (this->proxy_flow) {
		return true; // unclear
	}

	BOOST_LOG(l.d()) << "Verifying variables";

	auto sol = this->get_solution();
	for (unsigned int jid_a = 0; jid_a < this->instance.job_count(); ++jid_a) {
		const Job & job_a = this->instance.get_job(jid_a);
		unsigned int start_a = sol.get_start_time(jid_a);
		unsigned int end_a = start_a + job_a.get_duration();

		std::set<unsigned int> to_jobs;
		for (const auto & [to_jid, dummy] : this->sequence_vars[jid_a]) {
			to_jobs.insert(to_jid);
		}

		for (unsigned int r = 0; r < this->instance.resource_count(); ++r) {
			std::set<unsigned int> r_to_jobs;
			for (const auto & [to_jid, dummy] : this->flow_vars[r][jid_a]) {
				r_to_jobs.insert(to_jid);
			}

			if (r_to_jobs != to_jobs) {
				BOOST_LOG(l.e()) << "To-Jobs differ for From-Job " << jid_a
				                 << " and resource " << r;
				assert(false);
				return false;
			}
		}

		for (unsigned int jid_b = 0; jid_b < this->instance.job_count(); ++jid_b) {
			unsigned int start_b = sol.get_start_time(jid_b);

			if (start_b >= end_a) {
				if (to_jobs.find(jid_b) == to_jobs.end()) {
					BOOST_LOG(l.e()) << "Job " << jid_b << " ends after " << jid_a
					                 << " but there is no sequence variable!";
					assert(false);
					return false;
				}
			}
		}
	}

	return true;
}

// explicit instantiation

#if defined(GUROBI_FOUND)
template class FBILP<ilpabstraction::GurobiInterface>;
#endif

#if defined(CPLEX_FOUND)
template class FBILP<ilpabstraction::CPLEXInterface>;
#endif
