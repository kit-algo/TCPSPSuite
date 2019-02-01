/*
 * Start-Overlap Based MIP
 *
 * soilp.hpp
 *
 *  Created on: Jul 18, 2018
 *      Author: lukas
 */

#ifndef SRC_ILP_SOILP_HPP_
#define SRC_ILP_SOILP_HPP_

#include "../manager/solvers.hpp" // for get_free_N
#include "generated_config.hpp"   // for GUROBI_FOUND
#include "ilp.hpp"                // for ILPBase
#include "util/log.hpp"           // for Log
#include <string>                 // for string
#include <vector>                 // for vector
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
class SOILP : public ILPBase<SolverT> {
public:
  SOILP(const Instance & instance, AdditionalResultStorage & additional,
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
   * "Job j completely after i" variables
   *
   * after_vars[i][j] must be zero if j does not start after i finishes.
   */
  std::vector<std::vector<Variable>> after_vars;

  /*
   * "Job i starts before (or at the same time as) job i"
   *
   * before_vars[i][j] must be 1 if the start time of 1 is less or equal
   * than the start time of j.
   */
  std::vector<std::vector<Variable>> before_vars;

  /*
   * Usage expressions.
   *
   * start_usage[rid][j] is lower-bounded by the amount of resource <rid>
   * that is at the moment in which job j is started.
   */
  std::vector<std::vector<Expression>> start_usage;

  // TODO FIXME DEBUG ONLY
  std::vector<std::vector<Variable>> start_usage_var;

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
    solvers::get_free_N<SOILP<ilpabstraction::GurobiInterface>>()>
{
  constexpr static unsigned int my_N =
      solvers::get_free_N<SOILP<ilpabstraction::GurobiInterface>>();

  auto
  operator()()
  {
    return solvers::register_class<SOILP<ilpabstraction::GurobiInterface>,
                                   my_N>{}();
  }
};
#endif

#if defined(CPLEX_FOUND)
template <>
struct registry_hook<
    solvers::get_free_N<SOILP<ilpabstraction::CPLEXInterface>>()>
{
  constexpr static unsigned int my_N =
      solvers::get_free_N<SOILP<ilpabstraction::CPLEXInterface>>();

  auto
  operator()()
  {
    return solvers::register_class<SOILP<ilpabstraction::CPLEXInterface>,
                                   my_N>{}();
  }
};
#endif
} // namespace solvers

#endif /* SRC_ILP_SOILP_HPP_ */
