#ifndef ILP_FBILP_HPP
#define ILP_FBILP_HPP

#include "../manager/solvers.hpp"
#include "generated_config.hpp" // for GUROBI_FOUND
#include "ilp.hpp"              // for ILPBase
#include "../util/log.hpp"         // for Log

class AdditionalResultStorage;
class Instance;
class Job;
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
class FBILP : public ILPBase<SolverT> {
public:
	FBILP(const Instance & instance, AdditionalResultStorage & additional,
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

	const bool proxy_flow;
	std::vector<std::optional<const Job *>> proxies;

	// sequence_vars[i][j] becomes 1 only if i finishes before j starts
	std::vector<std::unordered_map<unsigned int, Variable>> sequence_vars;
	// flow_vars[r][i][j] denotes the amout of flow for resource r that flows
	// from i to j
	std::vector<std::vector<std::unordered_map<unsigned int, Variable>>>
	    flow_vars;
	// inflow_vars[r][i] denotes the amount of resource r that i does not receive
	// from other jobs
	std::vector<std::vector<Variable>> inflow_vars;

	void prepare_variables();
	void prepare_sequence_constraints();
	void prepare_flow_constraints();
	void prepare_objective();

	void prepare();

	bool dbg_verify_variables();
};

// Register the solver
namespace solvers {
#if defined(GUROBI_FOUND)
template <>
struct registry_hook<
    solvers::get_free_N<FBILP<ilpabstraction::GurobiInterface>>()>
{
	constexpr static unsigned int my_N =
	    solvers::get_free_N<FBILP<ilpabstraction::GurobiInterface>>();

	auto
	operator()()
	{
		return solvers::register_class<FBILP<ilpabstraction::GurobiInterface>,
		                               my_N>{}();
	}
};
#endif

#if defined(CPLEX_FOUND)
template <>
struct registry_hook<
    solvers::get_free_N<FBILP<ilpabstraction::CPLEXInterface>>()>
{
	constexpr static unsigned int my_N =
	    solvers::get_free_N<FBILP<ilpabstraction::CPLEXInterface>>();

	auto
	operator()()
	{
		return solvers::register_class<FBILP<ilpabstraction::CPLEXInterface>,
		                               my_N>{}();
	}
};
#endif
} // namespace solvers

#endif
