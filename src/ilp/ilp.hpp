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

// TODO SOS1-Constraint statt Big-M?

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

	Solution get_solution();
  static std::string get_id();
  Maybe<double> get_lower_bound();

  static const Traits &get_requirements();

protected:
	const Instance &instance;

  void print_profile() const;

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

	void prepare_pre();
	void prepare_post();

	void prepare_extension_constraints();

	// constraint (4)
  void prepare_edge_constraints();

	// constraints 7-9 are upheld automatically

  // objective (3)
  void prepare_objective();

	void do_initialization();
  void solve(Maybe<unsigned int> time_limit);
  void compute_values();

	MIPSolver env;
	Model model;

	// Start points
  std::vector<Variable> start_points;

  // Duration variables
  std::vector<Maybe<Variable>> duration_variables;

  // Maximum usage of the resources
  std::vector<Variable> max_usage_variables;

  // Overshoot of each resource in every timestep
  std::vector<std::vector<Variable>> overshoot_variables;

	// TODO totally switch these three off
	// Window-left-extension variables
	std::vector<Variable> left_extension_var;

	// Window-right-extension variables
	std::vector<Variable> right_extension_var;

	// Window-not-modified indicators
	std::vector<Variable> window_not_modified_var;

	Variable overshoot_cost_variable;
	Variable investment_cost_variable;

  unsigned int earliest_release;
  unsigned int latest_deadline;

  int timelimit;
  int seed;
  bool optimized;

	bool initialize_with_early;

  static const Traits required_traits;

  AdditionalResultStorage & additional_storage;
  const SolverConfig & sconf;

	Timer timer;
	Callback cb;

  Log l;

	Variable window_extension_time_var;
	Variable window_extension_job_var;
	Constraint window_extension_time_constraint;
	Constraint window_extension_job_constraint;
};

#endif
