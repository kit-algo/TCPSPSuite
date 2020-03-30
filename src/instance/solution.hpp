#ifndef SOLUTION_HPP
#define SOLUTION_HPP

#include "../datastructures/maybe.hpp" // for Maybe
#include "../util/log.hpp"             // for Log

#include <vector> // for vector
class InconsistentResultError;
class Instance;

/**
 * @brief a soultion for a TCPSP instance
 *
 * A solution contains for every job its start time.
 * most of the informations get calculated on request
 */
class Solution {
public:
	// TODO FIXME is this still "optimal?"

	/**
	 * Constructs a new Solution based on a vector-of-maybe
	 *
	 * @param Instance     The instance that was solved
	 * @param optimal      True if this Solution is optimal
	 * @param start_times  The start times for each job
	 * @param lower_bound  lower bound found for the TCPSP instance
	 */
	Solution(const Instance & instance, bool optimal,
	         std::vector<Maybe<unsigned int>> & start_times,
	         Maybe<double> lower_bound);

	/**
	 * Constructs a new Solution based on a vector-of-maybe
	 *
	 * @param Instance     The instance that was solved
	 * @param optimal      True if this Solution is optimal
	 * @param start_times  The start times for each job
	 * @param lower_bound  lower bound found for the TCPSP instance
	 */
	Solution(const Instance & instance, bool optimal,
	         std::vector<Maybe<unsigned int>> && start_times,
	         Maybe<double> lower_bound);

	/**
	 * Constructs a new Solution based on a vector-of-uint
	 *
	 * **Warning** for this constructor all jobs must have been scheduled!
	 *
	 * @param Instance     The instance that was solved
	 * @param optimal      True if this Solution is optimal
	 * @param start_times  The start times for each job
	 * @param lower_bound  lower bound found for the TCPSP instance
	 */
	Solution(const Instance & instance, bool optimal,
	         std::vector<unsigned int> & start_times, Maybe<double> lower_bound);

	/**
	 * "Default" constructor
	 * creates an obviously not feasible solution
	 */
	explicit Solution(const Instance & instance);

	// TODO this only here so Solution can be wrapped in a Maybe.
	Solution();

	// Copying & moving
	Solution(const Solution & other);
	Solution(Solution && other);
	Solution & operator=(const Solution & other);
	Solution & operator=(Solution && other);

	/**
	 * logs generell informations about this solution
	 * (for example the costs of this Solution)
	 */
	void print() const;

	/**
	 * logs for every job the time it starts and the last time it runs
	 */
	void print_jobs() const;

	/**
	 * logs the profile
	 */
	void print_profile() const;

	/**
	 * get the costs for this solution
	 *
	 * @return the actual cost or INF if the solution is not feasible
	 */
	double get_costs() const;
	double get_overshoot_costs() const;
	double get_investment_costs() const;

	/**
	 * Get the costs for this solution
	 *
	 * @return the actual cost
	 */
	double get_costs_lower_bound() const;

	/**
	 * Get the TCPSP instance that was solved
	 *
	 * @return the solved TCPSP instance
	 */
	const Instance * get_instance() const;

	/**
	 * Get the start time for the given job
	 * **Warning** If the job was not scheduled this is undefined!
	 * @param job_id the job to get the start time for
	 *
	 * @return the start time of the job
	 */
	unsigned int get_start_time(unsigned int job_id) const;

	/**
	 * checks if a job is scheduled
	 * @param job_id the job to check
	 *
	 * @return true if the job was scheduled
	 */
	bool job_scheduled(unsigned int job_id) const;

	/**
	 * checks if this solution is feasible
	 * **Warning** currently only checks if all jobs are scheduled
	 *
	 * @return true if the solutio is feasible
	 */
	bool is_feasible() const;

	/**
	 * checks if this solution is optimal
	 * **Warning** this is true if the solver says so!
	 *
	 * @return true if this solution is optimal
	 */
	bool is_optimal() const;

	/**
	 * Returns the maximum usage of the geven resource
	 * @param rid the resource id
	 *
	 * @return the maximum usage of the given resource
	 */
	double get_max_usage(unsigned int rid) const;

	/**
	 * get the lower bound for the TCPSP instance
	 *
	 * @return the lower bound
	 */
	Maybe<double> get_lower_bound() const;

	/**
	 * verifies that this solution is correct all constraints are met
	 */
	bool verify(int seed, InconsistentResultError * error_out = nullptr) const;

private:
	const Instance * instance;
	bool optimal;
	std::vector<Maybe<unsigned int>> start_times;
	Maybe<double> lower_bound;

	void compute_durations() const;
	mutable std::vector<unsigned int> durations;

	void compute_costs() const;
	mutable Maybe<double> costs;
	mutable Maybe<double> overshoot_costs;
	mutable Maybe<double> investment_costs;

	// TODO this is ugly
	mutable std::vector<double> max_usage;

	Log l;
};

#endif
