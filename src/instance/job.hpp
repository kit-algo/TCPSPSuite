#ifndef JOB_HPP
#define JOB_HPP

#include "resource.hpp"

/**
 * @brief A job of an TCPSP instance.
 *
 * A job of a TCPSP instance. Each job is associated with a set of attributes:
 *   - An id: Uniquely identifies this job.
 *   - A duration: Specifies for how long the job must be executed (without being interrupted).
 *   - A release: Specifies the first time step in which the job may be executed.
 *   - A deadline: Specifies the first time step in which the job must be finished.
 *   - One usage specification per resource: Specifies how much this job uses of a given resource during its execution.
 */
class Job {
public:
  using JobId = unsigned int;

  // TODO actually, this should not be default-constructible. We should adapt the way substitutions work
  Job();

  /**
   * Constructs a job from its release time, deadline, duration, usages and its ID.
   *
   * @param release   The release time of the job to be created
   * @param deadline  The deadline of the job to be created
   * @param duration  The duration of the job to be created
   * @param usages    A vector of usage specifications. usages[i] specifies how much of resource with ID i this job requires.
   * @param id        The ID of the job to be created.
   */
  Job(unsigned int release, unsigned int deadline, unsigned int duration, ResVec usages, unsigned int id);

  // Modified copy constructor that allows to change release and deadline during copying
  /**
   * Creates a job as a copy of another job (with the same ID!), allowing to modify release and deadline.
   *
   * **Warning**: Never add this job to the same instance as the <other> job. Both jobs have the same ID, and two jobs with the same ID
   * within the same instance produces undefined behavior.
   *
   * @param release   The release time of the new job
   * @param deadline  The deadline of the new job
   * @param other     The original job from which all other values are taken.
   */
  Job(unsigned int release, unsigned int deadline, const Job& other);

  /**
   * Returns the duration of the job.
   *
   * @return The duration of the job.
   */
  unsigned int get_duration() const;

  /**
   * Returns the release of the job.
   *
   * @return The release of the job.
   */
  unsigned int get_release() const;

  /**
   * Returns the deadline of the job.
   *
   * @return The deadline of the job.
   */
  unsigned int get_deadline() const;

  /**
   * Returns how much of a certain resource this job uses.
   *
   * @param rid   The ID of the resource one is interested about
   * @return The amount of resource with ID <rid> that this job uses
   */
  double get_resource_usage(unsigned int rid) const;

  /**
   * Returns how much of all resources this Job uses.
   *
   * @return the amount of resource used for each resource
   */
  const ResVec & get_resource_usage() const;

  /**
   * Returns the ID of the job.
   *
   * @return The ID of the job.
   */
  unsigned int get_jid() const;

  /**
   * Sets the ID of the job.
   *
   * **Warning**: Changing the ID of the job after the lag graph has been initialized breaks the
   * instance. Only use this if you know what you are doing.
   *
   * @param id  The new ID of this job.
   */
  void set_id(unsigned int id);

  bool operator==(const Job & other) const {
    return other.jid == this->jid;
    // TODO in debug-mode, compare all values!
  }

  // deepcopy
  /**
   * Performs a deep copy of this job.
   *
   * @return A cloned version of this job.
   */
  Job clone() const;

private:
  unsigned int jid;

  ResVec resource_usage; // TODO only flat?
  unsigned int duration;
  unsigned int release;
  unsigned int deadline;
};

#endif
