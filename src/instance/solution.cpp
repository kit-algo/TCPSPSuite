#include "solution.hpp"
#include <assert.h>                                            // for assert
#include <ext/alloc_traits.h>                                  // for __allo...
#include <algorithm>                                           // for move, max
#include <cmath>                                               // for ceil
#include <functional>                                          // IWYU pragma: keep // for refere...
#include <limits>                                              // for numeri...
#include <memory>                                              // for allocator
#include <string>                                              // for stream...
#include <tuple>                                               // for tuple
#include <unordered_map>                                       // for unorde...
#include <unordered_set>
#include <vector>                                              // for vector
#include "../datastructures/maybe.hpp"                      // for Maybe
#include "../manager/errors.hpp"                               // for Incons...
#include "../util/fault_codes.hpp"                             // for FAULT_...
#include "generated_config.hpp"                                // for ENABLE...
#include "../instance/solution.hpp"  // for Solution
#include "../instance/instance.hpp"        // for Instance
#include "../util/log.hpp"                 // for Log
#include "../instance/job.hpp"                                    // for Job
#include "laggraph.hpp"                                        // for LagGra...
#include "resource.hpp"                                        // for Resource



Solution::Solution()
  : instance(nullptr), optimal(false), l("SOLUTION")
{}

Solution::Solution(const Instance &instance_in)
  : instance(&instance_in), optimal(false), l("SOLUTION")
{}

Solution::Solution(const Solution & other)
  : instance(other.instance), optimal(other.optimal), start_times(other.start_times), lower_bound(other.lower_bound), l("SOLUTION")
{}

// TODO move mutables?
Solution::Solution(Solution && other)
  : instance(other.instance), optimal(other.optimal), start_times(std::move(other.start_times)), lower_bound(std::move(other.lower_bound)), l("SOLUTION")
{}

Solution::Solution(const Instance &instance_in, bool optmial_in, std::vector<Maybe<unsigned int>> & start_times_in, Maybe<double> lower_bound_in)
  : instance(&instance_in), optimal(optmial_in), start_times(start_times_in), lower_bound(lower_bound_in), l("SOLUTION")
{
  this->compute_durations();
}

Solution::Solution(const Instance &instance_in, bool optmial_in, std::vector<Maybe<unsigned int>> && start_times_in, Maybe<double> lower_bound_in)
  : instance(&instance_in), optimal(optmial_in), start_times(start_times_in), lower_bound(lower_bound_in), l("SOLUTION")
{
  this->compute_durations();
}

Solution::Solution(const Instance &instance_in, bool optmial_in, std::vector<unsigned int> & start_times_in, Maybe<double> lower_bound_in)
  : instance(&instance_in), optimal(optmial_in), lower_bound(lower_bound_in), l("SOLUTION")
{
  std::for_each(start_times_in.begin(), start_times_in.end(), [&](const auto & t) { this->start_times.push_back(t); });
  this->compute_durations();
}

Solution &
Solution::operator=(const Solution & other)
{
  this->instance = other.instance;
  this->optimal = other.optimal;
  this->start_times = other.start_times;
  this->lower_bound = other.lower_bound;

  // TODO mutables?
  this->durations.clear();
  this->costs = Maybe<double>();

  return *this;
}

Solution &
Solution::operator=(Solution && other)
{
  this->instance = other.instance;
  this->optimal = other.optimal;
  this->start_times = std::move(other.start_times);
  this->lower_bound = std::move(other.lower_bound);

  // TODO mutables?
  this->durations.clear();
  this->costs = Maybe<double>();

  return *this;
}

Maybe<double>
Solution::get_lower_bound() const
{
  return this->lower_bound;
}

void
Solution::compute_durations() const
{
  this->durations.clear();
  this->durations.resize(this->instance->job_count());

  for (unsigned int jid = 0 ; jid < this->instance->job_count() ; ++jid) {
    double duration = this->instance->get_job(jid).get_duration();
    if (!this->job_scheduled(jid)) {
      continue;
    }

    for (const auto & edge : this->instance->get_laggraph().reverse_neighbors(jid)) {
      if (edge.max_recharge > 0) {
        assert(edge.t == jid);

        if (!this->job_scheduled(edge.s)) {
          continue;
        }

        double addition = ((int)this->start_times[edge.s].value() - (int)this->start_times[edge.t].value() - (int)edge.lag) * edge.drain_factor;

        addition = std::max(addition, (double)edge.max_recharge);

        duration += addition;
      }
    }

    this->durations[jid] = (unsigned int)std::ceil(duration);
  }
}

// TODO should that seed be passed in the constructor?
bool
Solution::verify(int seed, InconsistentResultError * error_out) const
{
  // TODO completeness!

  // Step 0: Compute actual durations
  if (this->durations.size() == 0) {
    this->compute_durations();
  }

	if ((this->instance->get_window_extension_limit() == 0) &&
					(this->instance->get_window_extension_job_limit() == 0)) {
		// No window extension

		// Step 1: Verify that release / deadline constraints are met
		for (unsigned int j = 0 ; j < this->instance->job_count() ; ++j) {
			if ((unsigned int)this->start_times[j] < this->instance->get_job(j).get_release()) {
				if (error_out != nullptr) {
					*error_out = InconsistentResultError(*(this->instance), seed, FAULT_START_BEFORE_RELEASE, std::string("Job ") + std::to_string(j) + " started before its release.");
				}
				return false;
			}
			if (this->start_times[j] + this->durations[j] > this->instance->get_job(j).get_deadline()) {
				if (error_out != nullptr) {
					*error_out = InconsistentResultError(*(this->instance), seed, FAULT_END_AFTER_DEADLINE, std::string("Job ") + std::to_string(j) + " continuing after its deadline.");
				}
				return false;
			}
		}
	} else {
		// Window extension

		// Step 1: Verify that windows weren't extended too much
		unsigned int window_extension_sum = 0;
		unsigned int window_extension_job_sum = 0;

		for (unsigned int j = 0 ; j < this->instance->job_count() ; ++j) {
			if ((unsigned int)this->start_times[j] < this->instance->get_job(j).get_release()) {
				window_extension_sum += this->instance->get_job(j).get_release()
															  - (unsigned int)this->start_times[j];
				window_extension_job_sum += 1;
			}
			if (this->start_times[j] + this->durations[j] > this->instance->get_job(j).get_deadline()) {
				window_extension_sum += (unsigned int)this->start_times[j] + this->durations[j]
				                        - this->instance->get_job(j).get_deadline();
				window_extension_job_sum += 1;
			}
		}

		if (window_extension_sum > this->instance->get_window_extension_limit()) {
			if (error_out != nullptr) {
				*error_out = InconsistentResultError(*(this->instance), seed, FAULT_WINDOW_EXTENSION_SUM,
								                              std::string("Window extension limit violated."));
			}
			return false;
		}

		if (window_extension_job_sum > this->instance->get_window_extension_job_limit()) {
			if (error_out != nullptr) {
				*error_out = InconsistentResultError(*(this->instance), seed, FAULT_WINDOW_EXTENSION_JOB_SUM,
			                              std::string("Window extension job limit violated."));
			}
			return false;
		}
	}


  // Step 2: Verify that precedence relations are upheld
  for (auto edge : this->instance->get_laggraph().edges()) {
    unsigned int &s = edge.s;
    unsigned int &t = edge.t;
    int &lag = edge.lag;

    if ((int)this->start_times[s] + lag > (int)this->start_times[t]) {
    	if (error_out != nullptr) {
    		*error_out = InconsistentResultError(*this->instance, seed, FAULT_START_BEFORE_LAG,
																						 std::string("Job ") + std::to_string(t)
    		                                     + " starts before its lag from job " + std::to_string(s));
    	}
    	return false;
    }
  }

  BOOST_LOG(l.i()) << "Solution is valid." ;
  return true;
}

void
Solution::print_jobs() const
{
  BOOST_LOG(l.d(2)) << ">>>>>>>>>>>> PRINTING JOBS >>>>>>>>>>>>" ;
  for (unsigned int j = 0 ; j < this->instance->job_count() ; ++j) {
    unsigned int start = this->start_times[j];
    BOOST_LOG(l.d(2)) << "Job " << j << ": \t[" << start << " \t-> " << start + this->durations[j] << ")" ;
  }
  BOOST_LOG(l.d(2)) << "<<<<<<<<<<<< PRINTING JOBS <<<<<<<<<<<<" ;
}

void
Solution::print_profile() const
{
  std::vector<std::tuple<unsigned int, bool, std::reference_wrapper<const Job>>> events;

  unsigned int latest_point = 0;

  for (unsigned int j = 0 ; j < this->instance->job_count() ; ++j) {
    events.push_back(std::make_tuple(this->start_times[j], true, std::cref(this->instance->get_job(j))));
    events.push_back(std::make_tuple(this->start_times[j] + this->durations[j], false, std::cref(this->instance->get_job(j))));
    latest_point = std::max(latest_point, this->start_times[j] + this->durations[j]);
  }

  std::vector<std::vector<double>> profile(this->instance->resource_count(), std::vector<double>(latest_point, 0.0));

  std::sort(events.begin(), events.end(), [&](const std::tuple<int, bool, std::reference_wrapper<const Job>> &lhs, const std::tuple<int, bool, std::reference_wrapper<const Job>> &rhs) {
    int t1, t2;
    bool start1, start2;

    t1 = std::get<0>(lhs);
    t2 = std::get<0>(rhs);

    start1 = std::get<1>(lhs);
    start2 = std::get<1>(rhs);

    const Job &j1 = std::get<2>(lhs).get();
    const Job &j2 = std::get<2>(rhs).get();

    if (t1 != t2) {
      return t1 < t2;
    } else if (start1 != start2) {
      return start2;
    } else {
      return j1.get_jid() < j2.get_jid();
    }
  });


  std::unordered_map<unsigned int, double> current;
  std::vector<double> overshoot_sums(this->instance->resource_count(), 0.0);

  std::vector<double> max_res_usage(this->instance->resource_count(), 0.0);
  std::vector<std::vector<Job::JobId>> max_usage_jobs(this->instance->resource_count());
  std::vector<unsigned int> max_usage_time(this->instance->resource_count(), 0);

  for (unsigned int r = 0 ; r < this->instance->resource_count() ; ++r) {
    current[r] = 0.0;
  }

  unsigned int last_t = std::get<0>(events[0]);

	std::vector<decltype(this->instance->get_resource(0).get_availability().begin())> availability_its;
  for (unsigned int rid = 0 ; rid < this->instance->resource_count() ; ++rid) {
    availability_its.push_back(this->instance->get_resource(rid).get_availability().begin());
  }

  std::unordered_set<Job::JobId> active_jobs;

  for (std::tuple<unsigned int, bool, const Job &> ev : events) {
    bool start;
    unsigned int t;

    t = std::get<0>(ev);
    start = std::get<1>(ev);
    const Job &j = std::get<2>(ev);

    if (start) {
      BOOST_LOG(l.d()) << "====> " << last_t << " -> " << t << ": Start of " << j.get_jid() ;
    } else {
      BOOST_LOG(l.d()) << "====> " << last_t << " -> " << t << ": End of " << j.get_jid() ;
    }

    for (unsigned int r = 0 ; r < this->instance->resource_count() ; ++r) {
      unsigned int subseg_start = last_t;
      while ((availability_its[r] + 1 !=
                this->instance->get_resource(r).get_availability().end()) &&
              ((availability_its[r] + 1)->first < t)) {
        unsigned int subseg_end = (availability_its[r] + 1)->first;
        BOOST_LOG(l.d()) << "~~~~~> Sub-Segment in Resource " << r << " up to " << subseg_end;
        overshoot_sums[r] += (subseg_end - subseg_start) *
                    std::max((current[r] - availability_its[r]->second), 0.0);
      }

      double free_amount = this->instance->get_resource(r).get_availability().get_flat_available();
      double interval_costs;
      if (free_amount >= current[r]) {
        interval_costs = 0;
      } else {
        interval_costs = apply_polynomial(this->instance->get_resource(r).get_overshoot_costs(), (current[r] - free_amount));
        interval_costs *= (t - last_t);
        overshoot_sums[r] += (current[r] - free_amount) * (t - last_t);
      }
      for (unsigned int t2 = last_t; t2 < t; t2++) {
        profile[r][t2] = current[r];
      }
      BOOST_LOG(l.d()) << "    Res " << r << ": " << current[r] << " (costs " << interval_costs << ")" ;
    }

    // Update values
    if (start) {
    	active_jobs.insert(j.get_jid());
    } else {
    	active_jobs.erase(j.get_jid());
    }
    for (unsigned int r = 0 ; r < this->instance->resource_count() ; ++r) {
      if (start) {
        current[r] += j.get_resource_usage(r);
      } else {
        current[r] -= j.get_resource_usage(r);
      }

      if (current[r] > max_res_usage[r]) {
      	max_res_usage[r] = current[r];
      	max_usage_time[r] = t;
      	max_usage_jobs[r] = std::vector<Job::JobId>(active_jobs.begin(), active_jobs.end());
      }
    }

    last_t = t;
  }

  std::vector<double> profile_overshoot_sum(this->instance->resource_count(), 0.0);


  for (unsigned int r = 0 ; r < this->instance->resource_count() ; ++r) {
    BOOST_LOG(l.d()) << "======> Overshoot sum for Res " << r << ": " << overshoot_sums[r] ;
    BOOST_LOG(l.d()) << "======> Profile Overshoot sum for Res " << r << ": " << profile_overshoot_sum[r] ;
  }

  for (unsigned int r = 0 ; r < this->instance->resource_count() ; ++r) {
    BOOST_LOG(l.d()) << "====> Maximum usage for Res " << r << ": " << max_res_usage[r] ;
    BOOST_LOG(l.d()) << "~~~~~~> At time " << max_usage_time[r];
    std::cout << "~~~~~~> Active Jobs: " << max_usage_jobs[r] << "\n";
  }
}


void
Solution::print() const
{
  if (!this->optimal) {
    BOOST_LOG(l.i()) << "Solution is not optimal!" ;
  }

  BOOST_LOG(l.i()) << " -> Total costs: " << this->get_costs() ;
  BOOST_LOG(l.i()) << " -> Investment costs: " << this->investment_costs.value() ;
  BOOST_LOG(l.i()) << " -> Overshoot costs: " << this->overshoot_costs.value() ;

  if (this->lower_bound.valid()) {
    BOOST_LOG(l.i()) << " -> Lower Bound: " << this->lower_bound.value() ;
  } else {
    BOOST_LOG(l.i()) << " -> Lower Bound: NONE" ;
  }

}

bool
Solution::is_optimal() const
{
  return this->optimal;
}

double
Solution::get_costs() const
{
  if (!this->is_feasible()) {
    //std::cout << "Infeasible!";
    return std::numeric_limits<double>::max();
  }

  return this->get_costs_lower_bound();
}


double
Solution::get_overshoot_costs() const
{
	if (!this->is_feasible()) {
		return std::numeric_limits<double>::max();
	}

	if (!this->costs.valid()) {
		this->compute_costs();
	}

	return this->overshoot_costs.value();
}

double
Solution::get_investment_costs() const
{
	if (!this->is_feasible()) {
		return std::numeric_limits<double>::max();
	}

	if (!this->costs.valid()) {
		this->compute_costs();
	}

	return this->investment_costs.value();
}


double
Solution::get_costs_lower_bound() const
{
  if (!this->costs.valid()) {
    this->compute_costs();
  }

  return this->costs.value();
}

double
Solution::get_max_usage(unsigned int rid) const
{
  if (!this->costs.valid()) {
    this->compute_costs();
  }

#ifdef ENABLE_ASSERTIONS
  assert(rid < this->instance->resource_count());
#endif

  return this->max_usage[rid];
}

void
Solution::compute_costs() const
{
  //std::cout << "Computing costs\n";
  std::vector<std::tuple<unsigned int, bool, std::reference_wrapper<const Job>>> events;

  // TODO store this somehow else
  /*
  if (! this->is_feasible()) {
    this->costs = Maybe<double>(std::numeric_limits<double>::max());
    this->overshoot_costs = Maybe<double>(std::numeric_limits<double>::max());
    this->investment_costs = Maybe<double>(std::numeric_limits<double>::max());
    return;
  }
  */

  if (this->durations.size() == 0) {
    this->compute_durations();
  }

  for (unsigned int j = 0 ; j < this->instance->job_count() ; ++j) {
    events.push_back(std::make_tuple(this->start_times[j], true, std::cref(this->instance->get_job(j))));
    events.push_back(std::make_tuple(this->start_times[j] + this->durations[j], false, std::cref(this->instance->get_job(j))));
  }

  std::sort(events.begin(), events.end(), [&](const std::tuple<int, bool, std::reference_wrapper<const Job>> &lhs, const std::tuple<int, bool, std::reference_wrapper<const Job>> &rhs) {
    int t1, t2;
    bool start1, start2;

    t1 = std::get<0>(lhs);
    t2 = std::get<0>(rhs);

    start1 = std::get<1>(lhs);
    start2 = std::get<1>(rhs);

    const Job &j1 = std::get<2>(lhs).get();
    const Job &j2 = std::get<2>(rhs).get();

    if (t1 != t2) {
      return t1 < t2;
    } else if (start1 != start2) {
      return start2;
    } else {
      return j1.get_jid() < j2.get_jid();
    }
  });

  std::vector<double> current(this->instance->resource_count(), 0);
  this->max_usage = std::vector<double>(this->instance->resource_count(), 0);

  unsigned int last_t = std::get<0>(events[0]);
  // accumulators
  double acc_overshoot_costs = 0.0;
  double acc_investment_costs = 0.0;

  std::vector<std::vector<std::pair<unsigned int, double>>::const_iterator> availability_its;
  for (unsigned int rid = 0 ; rid < this->instance->resource_count() ; ++rid) {
    availability_its.push_back(this->instance->get_resource(rid).get_availability().begin());
  }

  for (std::tuple<unsigned int, bool, const Job &> ev : events) {
    bool start;
    unsigned int t;
    //std::cout << "Event at " << t << "\n";

    t = std::get<0>(ev);
    start = std::get<1>(ev);
    const Job &j = std::get<2>(ev);

    // Compute costs for the last segment (if this is the start of a new segment)
    if (t > last_t) {
      //std::cout << "New segment! last_T is: " << last_t << " T is: " << t << "\n";
      for (unsigned int r = 0; r < this->instance->resource_count(); ++r) {
        //std::cout << "Resource " << r << "\n";
        unsigned int subseg_start = last_t;

        // Changes of the availability function within the segment
        while ((availability_its[r] + 1 !=
                this->instance->get_resource(r).get_availability().end()) &&
               ((availability_its[r] + 1)->first < t)) {
          unsigned int subseg_end = (availability_its[r] + 1)->first;
	        if (subseg_end > last_t) {
		        //std::cout << "~~~~~> Sub-Segment in Resource " << r << " up to " << subseg_end << "\n";
		        //std::cout << "Usage: " << current[r] << "\n";
		        double overshoot_here = std::max(0.0, (current[r] - availability_its[r]->second));
		        double overshoot_costs_per_timestep =
						        apply_polynomial(this->instance->get_resource(r).get_overshoot_costs(),
						                         overshoot_here);
		        //std::cout << "Costs per time step: " << overshoot_costs_per_timestep << "\n";
		        double segment_overshoot_costs =
						        (subseg_end - subseg_start) * overshoot_costs_per_timestep;
		        //std::cout << "Segment costs: " << segment_overshoot_costs << "\n";
		        acc_overshoot_costs += std::max(segment_overshoot_costs, 0.0);

		        // Update maximum
		        this->max_usage[r] =
						        std::max(this->max_usage[r], (current[r] - availability_its[r]->second));
		        //std::cout << "Max usage now " << this->max_usage[r] << "\n";
		        subseg_start = subseg_end;
	        }
          availability_its[r]++;
        }

        //std::cout << "Usage: " << current[r] << "\n";
        // End of the segment
        double overshoot_here = std::max(0.0, (current[r] - availability_its[r]->second));
        double overshoot_costs_per_timestep =
                apply_polynomial(this->instance->get_resource(r).get_overshoot_costs(),
                                 overshoot_here);
        //std::cout << "Costs per time step: " << overshoot_costs_per_timestep << "\n";
        double segment_overshoot_costs = (t - subseg_start) * overshoot_costs_per_timestep;
        //std::cout << "Segment start: " << subseg_start << " / Segment End: " << t << "\n";
        //std::cout << "Segment costs: " << segment_overshoot_costs << "\n";
        acc_overshoot_costs += std::max(segment_overshoot_costs, 0.0);

        // Update maximum
        this->max_usage[r] =
                std::max(this->max_usage[r], (current[r] - availability_its[r]->second));
	      //std::cout << "Max usage now " << this->max_usage[r] << "\n";
      }
    }

    // Update current values
    for (unsigned int r = 0 ; r < this->instance->resource_count() ; ++r) {
      if (start) {
        current[r] += j.get_resource_usage(r);
      } else {
        current[r] -= j.get_resource_usage(r);
      }
    }

    last_t = t;
  }

  // Compute investment costs
  for (unsigned int r = 0 ; r < this->instance->resource_count() ; ++r) {
    //Log::i("SOLUTION", "\nMaximum usage for resource ", r, " is: \t", this->max_usage[r]);

    acc_investment_costs += apply_polynomial(this->instance->get_resource(r).get_investment_costs(), this->max_usage[r]);
  }

  this->costs = Maybe<double>(acc_investment_costs + acc_overshoot_costs);
  this->overshoot_costs = Maybe<double>(acc_overshoot_costs);
  //std::cout << "Overshoot: " << acc_overshoot_costs << "\n";
  this->investment_costs = Maybe<double>(acc_investment_costs);
  //std::cout << "Investment: " << acc_investment_costs << "\n";
}

const Instance *
Solution::get_instance() const
{
  return this->instance;
}

unsigned int
Solution::get_start_time(unsigned int job_id) const
{
  return this->start_times[job_id];
}

bool
Solution::is_feasible() const
{
  for (unsigned int jid = 0 ; jid < this->instance->job_count() ; ++jid) {
    if (!this->job_scheduled(jid)) {
      return false;
    }
  }

  // TODO check more

  return true;
}

bool
Solution::job_scheduled(unsigned int job_id) const
{
  return this->start_times[job_id].valid();
}
