#ifndef TCPSPSUITE_DTILP_HPP
#define TCPSPSUITE_DTILP_HPP

#include "../manager/solvers.hpp"
#include "ilp.hpp"

#if defined(GUROBI_FOUND)
#include "../contrib/ilpabstraction/src/ilpa_gurobi.hpp"
#endif

#if defined(CPLEX_FOUND)
#include "../contrib/ilpabstraction/src/ilpa_cplex.hpp"
#endif

template <class MIPSolverT>
class DTILP : public ILPBase<MIPSolverT> {
public:
	DTILP(const Instance & instance, AdditionalResultStorage & additional,
	      const SolverConfig & sconf);

	void prepare_warmstart();
	void warmstart_with_fixed(std::vector<bool> fixed_jobs,
	                          std::vector<unsigned int> start_pos,
	                          unsigned int time_limit,
	                          Maybe<double> extension_time_usage_cost_coefficient,
	                          Maybe<double> extension_job_usage_cost_coefficient,
	                          Maybe<unsigned int> extension_time_limit,
	                          Maybe<unsigned int> extension_job_limit);
	void run();
	static std::string get_id();

private:
	Log l;

	using Base = ILPBase<MIPSolverT>;
	using MIPSolver = typename Base::MIPSolver;
	using Model = typename Base::Model;
	using Variable = typename Base::Variable;
	using Expression = typename Base::Expression;
	using VarType = typename Base::VarType;
	using ParamType = typename Base::ParamType;
	using ModelStatus = typename Base::ModelStatus;
	using Constraint = typename Base::Constraint;

	void prepare();

	// Switch-On-Variables
	std::vector<std::vector<Variable>> variables;

	/* First entry: First time step at which the job can be executed (i.e., the
	 * time step that the first switch-on variable is associated with). Second
	 * entry: The number of switch-on variables, i.e., the window size (taking
	 * possible window extension into account). */
	std::vector<std::pair<unsigned int, unsigned int>> time_step_bounds;

	// Sum up duration, force overduration
	void prepare_duration_constraint();

	// Overduration indicator variables
	std::vector<std::vector<Variable>> overduration_variables;

	// Overshoot of each resource in every timestep
	std::vector<std::vector<Variable>> overshoot_variables;

	// Overduration / switch-on ANDing
	// overduration_and_swon_variables[i][x][y] will indicate whether:
	// job i
	//   - has an overduration of at least y time steps
	//   - and is switched on at (job-relative) time step x
	std::vector<std::vector<std::vector<Variable>>>
	    overduration_and_swon_variables;

	void prepare_start_point_constraints();

	void prepare_variables();

	// constraint (5) modified s.t. it is not limited
	// by a value but by a variable
	void prepare_resource_constraints();

	// constraint (6)
	void prepare_job_constraints();

	// Overshoot costs
	void prepare_overshoot_costs();

	void prepare_extension_constraints();

	// TODO totally switch these three off
	// Window-left-extension variables
	// IMPLEMENTATION must define these
	std::vector<Variable> left_extension_var;

	// Window-right-extension variables
	// IMPLEMENTATION must define these
	std::vector<Variable> right_extension_var;

	Variable window_extension_time_var;
	Variable window_extension_job_var;
	Constraint window_extension_time_constraint;
	Constraint window_extension_job_constraint;

	/*
	 * Warmstart-related things
	 */
	std::vector<bool> job_is_fixed;
	void fix_job(unsigned int jid, unsigned int time);
	void unfix_job(unsigned int jid);

	/*
	 * Options
	 */
	bool use_sos1_for_starts;

	void print_profile() const;
};

// Register the solver
namespace solvers {
#if defined(GUROBI_FOUND)
template <>
struct registry_hook<
    solvers::get_free_N<DTILP<ilpabstraction::GurobiInterface>>()>
{
	constexpr static unsigned int my_N =
	    solvers::get_free_N<DTILP<ilpabstraction::GurobiInterface>>();

	auto
	operator()()
	{
		return solvers::register_class<DTILP<ilpabstraction::GurobiInterface>,
		                               my_N>{}();
	}
};
#endif

#if defined(CPLEX_FOUND)
template <>
struct registry_hook<
    solvers::get_free_N<DTILP<ilpabstraction::CPLEXInterface>>()>
{
	constexpr static unsigned int my_N =
	    solvers::get_free_N<DTILP<ilpabstraction::CPLEXInterface>>();

	auto
	operator()()
	{
		return solvers::register_class<DTILP<ilpabstraction::CPLEXInterface>,
		                               my_N>{}();
	}
};
#endif
} // namespace solvers

#endif // TCPSPSUITE_DTILP_HPP
