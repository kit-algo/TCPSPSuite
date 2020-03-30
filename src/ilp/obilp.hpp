#ifndef ILP_OBILP_HPP
#define ILP_OBILP_HPP

#include "../manager/solvers.hpp" // for get_free_N
#include "generated_config.hpp"   // for GUROBI_FOUND
#include "ilp.hpp"                // for ILPBase
#include "util/log.hpp"           // for Log

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
class OBILP : public ILPBase<SolverT> {
public:
	OBILP(const Instance & instance, AdditionalResultStorage & additional,
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
	 * Data to perform things that are only relevant for overlapping jobs only
	 * on them.
	 */
	struct Event
	{
		unsigned int jid;
		unsigned int time;
		bool start;
	};
	std::vector<Event> events;
	void generate_events() noexcept;

	// TODO rename in 'disjunct' and 'order'?
	/*
	 * These are the D_{i,j} variables from the paper, meaning "Job j completely
	 * after job i".
	 *
	 * after_vars[i][j] must be zero if j does not start after i finishes.
	 */
	std::vector<std::unordered_map<unsigned int, Variable>> disjunct_vars;

	/*
	 * These are the O_{i,j} variables from the paper, meaning "Job i
	 * starts before (or at the same time as) job j"
	 *
	 * before_vars[i][j] must be 1 if the start time of i is less or equal
	 * than the start time of j.
	 */
	std::vector<std::unordered_map<unsigned int, Variable>> order_vars;

	/*
	 * Usage expressions.
	 *
	 * start_usage[rid][j] is lower-bounded by the amount of resource <rid>
	 * that is at the moment in which job j is started.
	 */
	std::vector<std::vector<Expression>> start_usage;

	// TODO FIXME DEBUG ONLY
	std::vector<std::vector<Variable>> start_usage_var;
	std::vector<Constraint> usage_constraints;

	void prepare_variables();

	/* Force after_vars to 0 where applicable */
	void prepare_after_constraints();

	/* Force before_vars to 1 where applicable */
	void prepare_before_constraints();

	/* Create start_usage expressions, include them in the objective */
	void prepare_start_usage_exprs();

	void prepare();
};

// Register the solver
namespace solvers {
#if defined(GUROBI_FOUND)
template <>
struct registry_hook<
    solvers::get_free_N<OBILP<ilpabstraction::GurobiInterface>>()>
{
	constexpr static unsigned int my_N =
	    solvers::get_free_N<OBILP<ilpabstraction::GurobiInterface>>();

	auto
	operator()()
	{
		return solvers::register_class<OBILP<ilpabstraction::GurobiInterface>,
		                               my_N>{}();
	}
};
#endif

#if defined(CPLEX_FOUND)
template <>
struct registry_hook<
    solvers::get_free_N<OBILP<ilpabstraction::CPLEXInterface>>()>
{
	constexpr static unsigned int my_N =
	    solvers::get_free_N<OBILP<ilpabstraction::CPLEXInterface>>();

	auto
	operator()()
	{
		return solvers::register_class<OBILP<ilpabstraction::CPLEXInterface>,
		                               my_N>{}();
	}
};
#endif
} // namespace solvers

#endif /* ILP_OBILP_HPP */
