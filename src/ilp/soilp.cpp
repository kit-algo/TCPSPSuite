/*
 * soilp.cpp
 *
 *  Created on: Jul 18, 2018
 *      Author: lukas
 */

#include "soilp.hpp"
#include "../contrib/ilpabstraction/src/common.hpp" // for VariableType, Varia...
#include "../instance/instance.hpp"                 // IWYU pragma: keep
#include "../instance/job.hpp"                      // for Job
#include "ilp.hpp"                                  // for ILPBase

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
SOILP<SolverT>::SOILP(const Instance & instance_in,
                      AdditionalResultStorage & additional_in,
                      const SolverConfig & sconf_in)
    : ILPBase<SolverT>(instance_in, additional_in, sconf_in), l("SOILP")
{}

template <class SolverT>
std::string
SOILP<SolverT>::get_id()
{
  return "SOILP v0.1 (" + std::string(SolverT::NAME) + ")";
}

template <class SolverT>
void
SOILP<SolverT>::prepare_variables()
{
  this->generate_vars_start_points();

  this->after_vars.resize(this->instance.job_count());
  for (unsigned int i = 0; i < this->instance.job_count(); ++i) {
    for (unsigned int j = 0; j < this->instance.job_count(); ++j) {
      this->after_vars[i].push_back(this->model.add_var(
          ilpabstraction::VariableType::BINARY, 0, 1,
          std::to_string(j) + std::string("_starts_after_") +
              std::to_string(i)));
    }
  }

  this->before_vars.resize(this->instance.job_count());
  for (unsigned int i = 0; i < this->instance.job_count(); ++i) {
    for (unsigned int j = 0; j < this->instance.job_count(); ++j) {
      this->before_vars[i].push_back(this->model.add_var(
          ilpabstraction::VariableType::BINARY, 0, 1,
          std::to_string(i) + std::string("_starts_before_") +
              std::to_string(j)));
    }
  }
}

template <class SolverT>
void
SOILP<SolverT>::prepare_after_constraints()
{
  for (unsigned int i = 0; i < this->instance.job_count(); ++i) {
    const Job & job_i = this->instance.get_job(i);

    for (unsigned int j = 0; j < this->instance.job_count(); ++j) {
      this->model.add_constraint(
          SolverT::NEGATIVE_INFTY, this->after_vars[i][j],
          1 + ((this->start_points[j] - this->start_points[i] -
                job_i.get_duration()) /
               this->latest_deadline),
          std::string("constr_") + std::to_string(j) + std::string("_after_") +
              std::to_string(i));
    }
  }
}

template <class SolverT>
void
SOILP<SolverT>::prepare_before_constraints()
{
  for (unsigned int i = 0; i < this->instance.job_count(); ++i) {
    for (unsigned int j = 0; j < this->instance.job_count(); ++j) {
      this->model.add_constraint(
          (this->start_points[j] - this->start_points[i] + 1) /
              this->latest_deadline,
          this->before_vars[i][j], SolverT::INFTY,
          std::string("constr_") + std::to_string(i) + std::string("_before_") +
              std::to_string(j));
    }
  }
}

template <class SolverT>
void
SOILP<SolverT>::prepare_start_usage_exprs()
{
  this->start_usage.resize(this->instance.resource_count());
  this->start_usage_var.resize(this->instance.resource_count());

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

	expr += (this->before_vars[i][jid] - this->after_vars[i][jid]) *
	        i_job.get_resource_usage(rid);
      }

      this->start_usage[rid].push_back(expr);

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

      this->model.add_constraint(
          expr, this->max_usage_variables[rid], SolverT::INFTY,
          std::string("usage_res_") + std::to_string(rid) +
              std::string("_start_of_") + std::to_string(jid));
    }
  }
}

template <class SolverT>
void
SOILP<SolverT>::prepare()
{
  this->prepare_pre();

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
SOILP<SolverT>::run()
{
  this->prepare();
  this->base_run();
}

// explicit instantiation

#if defined(GUROBI_FOUND)
template class SOILP<ilpabstraction::GurobiInterface>;
#endif

#if defined(CPLEX_FOUND)
template class SOILP<ilpabstraction::CPLEXInterface>;
#endif
