//
// Created by lukas on 10.07.18.
//

#ifndef TCPSPSUITE_EBILP_HPP
#define TCPSPSUITE_EBILP_HPP

#include "../manager/solvers.hpp" // for get_free_N
#include "../util/log.hpp"        // for Log
#include "generated_config.hpp"   // for GUROBI_FOUND
#include "ilp.hpp"                // for ILPBase

#include <string> // for string
#include <vector> // for vector
class AdditionalResultStorage;
class Instance;
class SolverConfig;
namespace solvers {
template <unsigned int>
struct registry_hook;
}

#if defined(GUROBI_FOUND)
#include "../contrib/ilpabstraction/src/ilpa_gurobi.hpp"
#endif

#if defined(CPLEX_FOUND)
#include "../contrib/ilpabstraction/src/ilpa_cplex.hpp"
#endif

template <class SolverT>
class EBILP : public ILPBase<SolverT> {
public:
	EBILP(const Instance & instance, AdditionalResultStorage & additional,
	      const SolverConfig & sconf);

	void run();
	static std::string get_id();

private:
	Log l;

	using Base = ILPBase<SolverT>;
	using MIPSolver = typename Base::MIPSolver;
	using Model = typename Base::Model;
	using Variable = typename Base::Variable;
	using Expression = typename Base::Expression;
	using VarType = typename Base::VarType;
	using ParamType = typename Base::ParamType;
	using ModelStatus = typename Base::ModelStatus;
	using Constraint = typename Base::Constraint;

	/*
	 * Job-Start <=> Event Mapping
	 *
	 * job_start_events[j][a] becomes 1 if and only if job j starts at event
	 * skip_numbers[j].first + a.
	 */
	std::vector<std::vector<Variable>> job_start_events;

	/*
	 * Job-End <=> Event Mapping
	 *
	 * job_end_events[j][a] becomes 1 if and only if job j ends at event
	 * skip_numbers[j].first + a.
	 */
	std::vector<std::vector<Variable>> job_end_events;

	/*
	 * event_times[a] is the point in time at which event a happens
	 */
	std::vector<Variable> event_times;

	/*
	 * end_points[jid] is the time when job jid finishes
	 * TODO a variable should not be necessary here
	 */
	// std::vector<Variable> end_points;

	/*
	 * usages_after_event[rid][a] contains the amount of resource <rid> used after
	 * event <a>
	 */
	// std::vector<std::vector<Variable>> usages_after_event;

	/*
	 * event_usages[rid][a] holds the amount of resource <rid> used after event
	 * <a>
	 */
	std::vector<std::vector<Expression>> event_usages;

	/*
	 * Skip numbers specify how many of the first and last events
	 * can be skipped for each job, saving us job_start_events and job_end_events
	 * variables (and constraints).
	 *
	 * The number of jobs that *must* be finished before j (times 2) is the
	 * number of events to skip in the beginning, the number of jobs that must
	 * start after j finishes is the number of events to skip in the end.
	 */
	std::vector<std::pair<size_t, size_t>> skip_numbers;
	std::vector<size_t> event_counts;

	unsigned int max_deadline;

	/* True if we use (and set) the start points in the base ILP
	 */
	bool start_point_mode;

	/* Set to true if we use a sum trick to enforce "end after start"
	 * for every job. */
	bool enforce_end_after_start_via_sum;

	void compute_skip_numbers() noexcept;

	void prepare_variables();

	/* For now: every job's duration variable is set fixed to its duration */
	void prepare_duration_constraint();

	/* Enforce event ordering. TODO is this necessary? */
	void prepare_event_constraints();

	/* Make sure every event is used once, and every job has exactly one start and
	 * end */
	void prepare_job_event_constraints();

	/* Links events and job durations */
	void prepare_time_constraints();

	/* Set start_points variables in base ILP */
	void prepare_start_points_constraints();

	/* Prepare realease / deadline constraints (if start points are not used) */
	void prepare_release_deadline_constraints();

	/* Prepare dependency constraints (if start points are not used) */
	void prepare_dependency_constraints();

	/* Create event_usages expressions */
	void prepare_usage_expressions();

	void prepare();

	/* Only necessary if start points are not used */
	void create_solution();
};

// Register the solver
namespace solvers {
#if defined(GUROBI_FOUND)
template <>
struct registry_hook<
    solvers::get_free_N<EBILP<ilpabstraction::GurobiInterface>>()>
{
	constexpr static unsigned int my_N =
	    solvers::get_free_N<EBILP<ilpabstraction::GurobiInterface>>();

	auto
	operator()()
	{
		return solvers::register_class<EBILP<ilpabstraction::GurobiInterface>,
		                               my_N>{}();
	}
};
#endif

#if defined(CPLEX_FOUND)
template <>
struct registry_hook<
    solvers::get_free_N<EBILP<ilpabstraction::CPLEXInterface>>()>
{
	constexpr static unsigned int my_N =
	    solvers::get_free_N<EBILP<ilpabstraction::CPLEXInterface>>();

	auto
	operator()()
	{
		return solvers::register_class<EBILP<ilpabstraction::CPLEXInterface>,
		                               my_N>{}();
	}
};
#endif
} // namespace solvers

#endif // TCPSPSUITE_EBILP_HPP
