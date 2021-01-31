#ifndef INSTANCE_HPP
#define INSTANCE_HPP

#include "job.hpp"    // for Job
#include "traits.hpp" // for Traits

#include <cstddef>  // for ptrdiff_t, size_t
#include <iterator> // for input_iterator_tag
#include <memory>   // IWYU pragma: keep
#include <string>   // for string
#include <vector>   // for vector
class LagGraph;
class Resource;
class ResVec;

/**
 * @brief a TCPSP instance
 *
 * A TCPSP instance consists of the folowing data:
 *   - a list of jobs that need to be scheduled
 *   - a list of ressources used by the jobs
 *   - a name
 */
class Instance {
public:
	class JobContainer {
	public:
		JobContainer(const Instance * instance);

		class iterator {
		public:
			typedef std::input_iterator_tag iterator_category;
			typedef Job value_type;
			typedef Job * pointer;
			typedef Job & reference;
			typedef size_t size_type;
			typedef std::ptrdiff_t difference_type;

			iterator(const JobContainer & c);
			iterator(const JobContainer & c, unsigned int pos);

			const value_type & operator*();
			const value_type * operator->();

			iterator operator++(int);
			iterator & operator++();
			iterator operator--(int);
			iterator & operator--();

			bool operator==(const iterator & other) const;
			bool operator!=(const iterator & other) const;

		private:
			const JobContainer & c;
			unsigned int i;
		};

		iterator begin() const;
		iterator end() const;

		bool operator==(const JobContainer & other) const;

	private:
		const Instance * instance;
	};

	// copy & move assignment / construction need to *not* copy the cached
	// container
	void swap(Instance & other);
	Instance & operator=(Instance other);
	Instance(const Instance & other);
	Instance(Instance && other);

	/**
	 * Default constructor
	 * creates an empty instance without an id
	 */
	Instance();

	/**
	 * Constructs a new instance with a name and sets
	 * the instance should have
	 *
	 * @param instance_id the id of this instance
	 * @param wanted_traits the traits this should have
	 */
	Instance(const std::string instance_id, Traits wanted_traits);

	// copy-and-substitute
	/**
	 * Constructs a new instance based on another instance
	 *
	 * @param origin the instance this should be based on
	 * @param job_is_substituted a vector-of-bool stating
	 *        for every job if it is substituted or not
	 * @param substitutions the substitutions
	 */
	Instance(const Instance & origin, std::vector<bool> && job_is_substituted,
	         std::vector<Job> && substitutions);

	~Instance();

	/**
	 * Returns the lag graph
	 *
	 * @return the lag graph
	 */
	LagGraph & get_laggraph();

	/**
	 * Returns the lag graph
	 *
	 * @return the lag graph
	 */
	const LagGraph & get_laggraph() const;

	/**
	 * adds a job to this instance
	 * @param job that should be added
	 *
	 * @return the index of the job
	 */
	unsigned int add_job(Job && job);

	/**
	 * adds a resource to this instance
	 * @param resource that should be added
	 *
	 * @return the index of the resource
	 */
	unsigned int add_resource(Resource && resource);

	/**
	 * Returns the number of jobs this instance has
	 *
	 * @return the number of jobs this instance has
	 */
	unsigned int job_count() const;

	/**
	 * Returns the job with the given index
	 *
	 * @return the job with the given index
	 */
	const Job & get_job(unsigned int i) const;

	/**
	 * Returns a container with all jobs of this instance
	 *
	 * @return a container with all jobs of this instance
	 */
	const JobContainer & get_jobs() const;

	/**
	 * Returns the number of resourcec this instance has
	 *
	 * @return the number of resourcec this instance has
	 */
	unsigned int resource_count() const;

	/**
	 * Returns the resource with the given index
	 *
	 * @return the resource with the given index
	 */
	const Resource & get_resource(unsigned int i) const;

	/**
	 * Returns all traits this instance fulfills (not only wanted ones)
	 *
	 * @return all traits this instance fulfills (not only wanted ones)
	 */
	const Traits & get_traits() const;

	/**
	 * computes the traits this instance actually fulfills
	 * **Warning** this throws an assertion error if the
	 *             wanted traits are not fulfilled!
	 */
	void compute_traits();

	/**
	 * Returns the id of this instance
	 *
	 * @return the id of this instance
	 */
	const std::string & get_id() const;

	/**
	 * Returns a deep copy of this instance
	 *
	 * @return a deep copy of this instance
	 */
	Instance clone() const;

	/**
	 * checks if the created instance is feasible
	 * if CRASH_ON_CHECK this will throw an assertion error
	 * instead of returning!
	 *
	 * @returns true if it possible to schedule all jobs
	 */
	bool check_feasibility() const;

	/**
	 * @brief Setter for the window extension parameters
	 *
	 * @param window_extension_limit		 The total number of time steps
	 * that windows may be extended
	 * @param window_extension_job_limit The maximum number of jobs that may have
	 * their windows extended
	 */
	void set_window_extension(unsigned int window_extension_limit,
	                          unsigned int window_extension_job_limit);
	void set_window_extension_hard_deadline(Maybe<unsigned int> deadline);
	
	unsigned int get_window_extension_limit() const;
	unsigned int get_window_extension_job_limit() const;
	Maybe<unsigned int> get_window_extension_hard_deadline() const;

	/**
	 * calculates the cost of a given solution in form of a vector of start times
	 * (it is NOT validated if the solution is correct!)
	 *
	 * **Warning** This metho currently uses flat resources!!!
	 * TODO change this!
	 *
	 * @param start_times the start times for each job
	 * @return the total costs of this solution
	 */
	double
	calculate_max_costs(const std::vector<unsigned int> & start_times) const;

	/**
	 * calculates the cost of a given resource usage
	 *
	 * **Warning** This metho currently uses flat resources!!!
	 * TODO change this!
	 *
	 * @param ressource_usage the base usage of all resources
	 * @param additional_usage the additional usage of all resources
	 *
	 * TODO what the hell? What is this function for?
	 *
	 * @return the overall costs
	 */
	double calculate_costs(const ResVec & ressource_usage,
	                       const ResVec & additional_usage) const;

	/**
	 * calculates the cost of a given resource usage
	 *
	 * **Warning** This metho currently uses flat resources!!!
	 * TODO change this!
	 *
	 * @param ressource_usage the base usage of all resources
	 *
	 * @return the overall costs
	 */
	double calculate_costs(const ResVec & ressource_usage) const;

	/**
	 * @brief Helper to return the latest deadline in the instance
	 *
	 * @return The latest deadline in the instance
	 */
	unsigned int get_latest_deadline() const;

private:
	// These things are shared across substituted instances
	std::shared_ptr<std::vector<Resource>> resources;
	std::shared_ptr<std::vector<Job>> jobs;
	std::shared_ptr<std::string> instance_id;
	std::shared_ptr<LagGraph> laggraph;

	std::vector<bool> job_is_substituted;
	std::vector<Job> substitutions;
	JobContainer cached_container;

	unsigned int window_extension_limit;
	unsigned int window_extension_job_limit;
	Maybe<unsigned int> window_extension_hard_deadline;

	Traits wanted_traits;
	Traits computed_traits;
};

#endif
