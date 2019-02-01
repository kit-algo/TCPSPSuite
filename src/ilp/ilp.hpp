#ifndef ILP_HPP
#define ILP_HPP

#include <iomanip>                                            // for operator<<
#include <string>                                             // for string
#include <vector>                                             // for vector
#include "../contrib/ilpabstraction/src/common.hpp"           // for ModelSt...
#include "../manager/timer.hpp"                               // for Timer
#include "../datastructures/maybe.hpp"              // for Maybe
#include "../instance/solution.hpp"                 // for Solution
#include "../util/log.hpp"                                // for Log
class AdditionalResultStorage;
class Instance;
class SolverConfig;
class Traits;

/* This is the basic discrete-time formulation of Kone et al. taken from
 * 10.1016/j.cor.2009.12.011
 *
 * The objective function was modified to optimize for RACP instead of RCPSP.
 * The (global only!) deadline is automatically upheld by the fact that the
 * start-indicator variable is only created up to the point where the last job must
 * start the latest.
 */
template <class MIPSolverT>
class ILPBase
{
public:
  ILPBase(const Instance &instance, AdditionalResultStorage & additional, const SolverConfig & sconf);
  ~ILPBase();

	using MIPSolver = MIPSolverT;
	using Model = typename MIPSolverT::Model;
	using Variable = typename MIPSolverT::Variable;
	using Expression = typename MIPSolverT::Expression;
	using VarType = ilpabstraction::VariableType;
	using ParamType = ilpabstraction::ParamType;
	using ModelStatus = ilpabstraction::ModelStatus;
	using Constraint = typename MIPSolverT::Constraint;
	using MIPFeatures = ilpabstraction::Features;

  Maybe<double> get_lower_bound();

  static const Traits &get_requirements();

  Solution get_solution();

protected:
	const Instance &instance;
  class Callback : public MIPSolverT::Callback {
  public:
    Callback(const Timer & timer, AdditionalResultStorage & additional_storage, Log & l);
    virtual void on_poll(typename MIPSolver::Callback::Context & ctx) override;
	  virtual void on_message(typename MIPSolver::Callback::Context & ctx, std::string & message) override;
  private:
    const double LOG_INTERVAL = 10; // seconds
		const int REPEAT_HEADER = 30;

    void log_intermediate();
    double last_intermediate_time = 0;

    const Timer & timer;
    AdditionalResultStorage & additional_storage;

	  double last_log;
	  int lines_before_header;
    Log & l;
  };

	void prepare_base_variables();
	void prepare_pre();
	void prepare_post();

	void base_run();

	// constraint (4)
  void prepare_edge_constraints();

	// constraints 7-9 are upheld automatically

  // objective (3)
  void prepare_objective();

	void do_initialization();
  void solve(Maybe<unsigned int> time_limit);
  void compute_values();

	Solution get_solution_by_start_vars();

	MIPSolver env;
	Model model;

	/* Solution
	 *
	 * Must be set by the implementation if start_points are not being used
	 */
	std::vector<Maybe<unsigned int>> computed_solution_start_times;

	/* Start points
	 * IMPLEMENTATION must define these for:
	 * 	 * job dependencies
	 *   * automatic solution retrieval
	 * If they are generated, implementation must make sure to introduce constraints
 	 * on them! */
	void generate_vars_start_points();
	bool start_points_set;
  std::vector<Variable> start_points;

  // Duration variables
  // IMPLEMENTATION must define these
  std::vector<Variable> duration_variables;

  // Maximum usage of the resources
  // IMPLEMENTATION must define these
  std::vector<Variable> max_usage_variables;

	// Window-not-modified indicators
	// IMPLEMENTATION must define these
	std::vector<Variable> window_not_modified_var;

	// IMPLEMENTATION must define these using constraints
	Variable overshoot_cost_variable;
	Variable investment_cost_variable;

  unsigned int earliest_release;
  unsigned int latest_deadline;

  int timelimit;
  int seed;
  bool optimized;

	bool initialize_with_early;
	bool collect_kappa_stats;

  static const Traits required_traits;

  AdditionalResultStorage & additional_storage;
  const SolverConfig & sconf;

	Timer timer;
	Callback cb;

  Log l;
};

#endif
