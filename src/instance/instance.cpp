#include "instance.hpp"
#include "../algorithms/graphalgos.hpp" // for CriticalP...
#include "generated_config.hpp"         // for CRASH_ON_...
#include "job.hpp"                      // for Job
#include "laggraph.hpp"                 // for LagGraph
#include "resource.hpp"                 // for Resource
#include "traits.hpp"                   // for Traits
#include <algorithm>                    // for move, swap
#include <assert.h>                     // for assert
#include <boost/container/vector.hpp>   // for vector
#include <limits>                       // for numeric_l...
#include <memory>                       // for __shared_ptr_access, __share...
#include <set>                          // for set
#include <tuple>                        // for get, make_tuple, tuple

Instance::Instance()
    : resources(std::make_shared<std::vector<Resource>>()),
      jobs(std::make_shared<std::vector<Job>>()),
      instance_id(std::make_shared<std::string>("")),
      laggraph(std::make_shared<LagGraph>()), job_is_substituted(),
      cached_container(this), window_extension_limit(0),
      window_extension_job_limit(0), wanted_traits(Traits()),
      computed_traits(Traits())
{}

Instance::Instance(const std::string instance_id_in, Traits wanted_traits_in)
    : resources(std::make_shared<std::vector<Resource>>()),
      jobs(std::make_shared<std::vector<Job>>()),
      instance_id(std::make_shared<std::string>(instance_id_in)),
      laggraph(std::make_shared<LagGraph>()), job_is_substituted(),
      cached_container(this), window_extension_limit(0),
      window_extension_job_limit(0), wanted_traits(wanted_traits_in),
      computed_traits(wanted_traits_in)
{}

Instance::Instance(const Instance & origin,
                   std::vector<bool> && job_is_substituted_in,
                   std::vector<Job> && substitutions_in)
    : resources(origin.resources), jobs(origin.jobs),
      instance_id(origin.instance_id), laggraph(origin.laggraph),
      job_is_substituted(
          std::forward<std::vector<bool>>(job_is_substituted_in)),
      substitutions(std::forward<std::vector<Job>>(substitutions_in)),
      cached_container(this), window_extension_limit(0),
      window_extension_job_limit(0), wanted_traits(origin.wanted_traits),
      computed_traits(origin.computed_traits)
{}

Instance::Instance(const Instance & origin)
    : resources(origin.resources), jobs(origin.jobs),
      instance_id(origin.instance_id), laggraph(origin.laggraph),
      job_is_substituted(origin.job_is_substituted),
      substitutions(origin.substitutions), cached_container(this),
      window_extension_limit(0), window_extension_job_limit(0),
      wanted_traits(origin.wanted_traits),
      computed_traits(origin.computed_traits)
{}

Instance::Instance(Instance && origin)
    : resources(std::move(origin.resources)), jobs(std::move(origin.jobs)),
      instance_id(std::move(origin.instance_id)),
      laggraph(std::move(origin.laggraph)),
      job_is_substituted(std::move(origin.job_is_substituted)),
      substitutions(std::move(origin.substitutions)), cached_container(this),
      window_extension_limit(0), window_extension_job_limit(0),
      wanted_traits(std::move(origin.wanted_traits)),
      computed_traits(std::move(origin.computed_traits))
{}

void
Instance::swap(Instance & other)
{
  std::swap(other.resources, this->resources);
  std::swap(other.jobs, this->jobs);
  std::swap(other.instance_id, this->instance_id);
  std::swap(other.laggraph, this->laggraph);
  std::swap(other.job_is_substituted, this->job_is_substituted);
  std::swap(other.window_extension_limit, this->window_extension_limit);
  std::swap(other.window_extension_job_limit, this->window_extension_job_limit);
  std::swap(other.substitutions, this->substitutions);
  std::swap(other.wanted_traits, this->wanted_traits);
  std::swap(other.computed_traits, this->computed_traits);
}

Instance &
Instance::operator=(Instance other)
{
  this->swap(other);
  return *this;
}

Instance
Instance::clone() const
{
  Instance cloned(*this->instance_id, this->wanted_traits.clone());

  // clone resources
  for (auto res : *this->resources) {
    cloned.add_resource(res.clone());
  }

  // clone jobs
  for (auto job : *this->jobs) {
    cloned.add_job(job.clone());
  }

  cloned.laggraph = std::make_shared<LagGraph>(this->laggraph->clone());

  cloned.set_window_extension(this->window_extension_limit,
                              this->window_extension_job_limit);

  cloned.computed_traits = this->computed_traits.clone();

  return cloned;
}

Instance::~Instance() {}

const std::string &
Instance::get_id() const
{
  return *this->instance_id;
}

const Instance::JobContainer &
Instance::get_jobs() const
{
  return this->cached_container;
}

unsigned int
Instance::add_job(Job && job)
{
  decltype(*this->jobs) & job_vec = *this->jobs;

  if ((job_vec.size() > 0) &&
      (this->wanted_traits.has_flag(Traits::COMMON_RELEASE))) {
    if (job.get_release() != this->get_job(0).get_release()) {
      throw TraitViolatedError("Release times must be aligned.\n");
    }
  }

  if ((job_vec.size() > 0) &&
      (this->wanted_traits.has_flag(Traits::COMMON_DEADLINE))) {
    if (job.get_deadline() != this->get_job(0).get_deadline()) {
      throw TraitViolatedError("Deadline times must be aligned.\n");
    }
  }

  if ((job_vec.size() > 0) &&
      (this->wanted_traits.has_flag(Traits::COMMON_DURATION))) {
    if (job.get_duration() != this->get_job(0).get_duration()) {
      throw TraitViolatedError("Duration times must be equal.\n");
    }
  }

  job_vec.push_back(job);
  job_vec[job_vec.size() - 1].set_id((unsigned int)(job_vec.size() - 1));

  unsigned int vertex = this->get_laggraph().add_vertex();

  this->job_is_substituted.push_back(false);

#ifdef ENABLE_ASSERTIONS
  assert(vertex == (unsigned int)(job_vec.size() - 1));
  assert(job_vec.size() < std::numeric_limits<unsigned int>::max());
  assert(this->job_is_substituted.size() == job_vec.size());
#endif

  return (unsigned int)(job_vec.size() - 1);
}

unsigned int
Instance::add_resource(Resource && resource)
{
  decltype(*this->resources) & res_vec = *this->resources;

  res_vec.push_back(resource);
  res_vec[res_vec.size() - 1].set_id((unsigned int)(res_vec.size() - 1));
  assert(res_vec.size() < std::numeric_limits<unsigned int>::max());
  return (unsigned int)(res_vec.size() - 1);
}

const Traits &
Instance::get_traits() const
{
  return this->computed_traits;
}

void
Instance::compute_traits()
{
  TraitsBuilder tb(*this);
  tb.run();
  this->computed_traits = tb.get_traits();

  // TODO make this throw in fulfills()
  assert(this->computed_traits.fulfills(this->wanted_traits));
}

LagGraph &
Instance::get_laggraph()
{
  return *this->laggraph;
}

const LagGraph &
Instance::get_laggraph() const
{
  return *this->laggraph;
}

unsigned int
Instance::job_count() const
{
  return (unsigned int)this->jobs->size();
}

const Job &
Instance::get_job(unsigned int i) const
{
  if (!this->job_is_substituted[i]) {
    return this->jobs->at(i);
  } else {
    return this->substitutions.at(i);
  }
}

unsigned int
Instance::resource_count() const
{
  return (unsigned int)this->resources->size();
}

const Resource &
Instance::get_resource(unsigned int i) const
{
  return this->resources->at(i);
}

Instance::JobContainer::JobContainer(const Instance * instance_in)
    : instance(instance_in)
{}

bool
Instance::JobContainer::operator==(const Instance::JobContainer & other) const
{
  return other.instance == this->instance;
}

Instance::JobContainer::iterator::iterator(const Instance::JobContainer & c_in)
    : c(c_in), i(0)
{}

Instance::JobContainer::iterator::iterator(const Instance::JobContainer & c_in,
                                           unsigned int pos)
    : c(c_in), i(pos)
{}

const Job & Instance::JobContainer::iterator::operator*()
{
  if (this->c.instance->job_is_substituted[this->i]) {
    return this->c.instance->substitutions[this->i];
  } else {
    return this->c.instance->jobs->at(this->i);
  }
}

const Job * Instance::JobContainer::iterator::operator->()
{
  return &(*(*this));
}

Instance::JobContainer::iterator
Instance::JobContainer::iterator::operator++(int)
{
  iterator tmp(*this);
  this->i++;
  return tmp;
}

Instance::JobContainer::iterator &
Instance::JobContainer::iterator::operator++()
{
  this->i++;
  return *this;
}

Instance::JobContainer::iterator
Instance::JobContainer::iterator::operator--(int)
{
  iterator tmp(*this);
  this->i--;
  return tmp;
}

Instance::JobContainer::iterator &
Instance::JobContainer::iterator::operator--()
{
  this->i--;
  return *this;
}

bool
Instance::JobContainer::iterator::operator==(const iterator & other) const
{
  return (other.i == this->i) && (other.c == this->c);
}

bool
Instance::JobContainer::iterator::operator!=(const iterator & other) const
{
  return !(other == *this);
}

Instance::JobContainer::iterator
Instance::JobContainer::begin() const
{
  return iterator(*this);
}

Instance::JobContainer::iterator
Instance::JobContainer::end() const
{
#ifdef OMG_VERIFY
  assert(this->instance->jobs->size() <
         std::numeric_limits<unsigned int>::max());
#endif
  return iterator(*this, (unsigned int)this->instance->jobs->size());
}

bool
Instance::check_feasibility() const
{
  CriticalPathComputer cpc(*this);
  std::vector<unsigned int> cp = cpc.get_forward();

  for (const auto & job : this->get_jobs()) {
    if (cp[job.get_jid()] > job.get_deadline() - job.get_duration()) {
#ifdef CRASH_ON_CHECK
      assert(false);
#endif
      return false;
    }
  }

  return true;
}

void
Instance::set_window_extension(unsigned int window_extension_limit_in,
                               unsigned int window_extension_job_limit_in)
{
  this->window_extension_limit = window_extension_limit_in;
  this->window_extension_job_limit = window_extension_job_limit_in;
}

unsigned int
Instance::get_window_extension_job_limit() const
{
  return this->window_extension_job_limit;
}

unsigned int
Instance::get_window_extension_limit() const
{
  return this->window_extension_limit;
}

double
Instance::calculate_max_costs(const std::vector<unsigned int> & solution) const
{
  double maxCost = 0;
  std::set<std::tuple<unsigned int, int, unsigned int>> events;
  for (const Job & job : get_jobs()) {
    // the -1 event has to happen before the +1 events for the same Time to
    // calculate the correct result!!! as we order by < this should always
    // happen...
    events.insert(std::make_tuple(solution[job.get_jid()], +1, job.get_jid()));
    events.insert(std::make_tuple(
        solution[job.get_jid()] + 1 + job.get_duration(), -1, job.get_jid()));
  }
  ResVec ressources(resource_count());
  for (auto event : events) {
    const Job & job = get_job(std::get<2>(event));
    for (unsigned int rid = 0; rid < ressources.size(); rid++) {
      ressources[rid] += std::get<1>(event) * job.get_resource_usage(rid);
    }
    maxCost = std::max(maxCost, calculate_costs(ressources));
  }
  return maxCost;
}

double
Instance::calculate_costs(const ResVec & ressource_usage,
                          const ResVec & additional_usage) const
{
  double sum = 0;
  for (unsigned int rid = 0; rid < resource_count(); rid++) {
    const Resource & resource = get_resource(rid);
    double usage = ressource_usage[rid] + additional_usage[rid] -
                   resource.get_availability().get_flat_available();
    if (usage > 0) {
      sum += apply_polynomial(resource.get_investment_costs(), usage);
      sum += apply_polynomial(resource.get_overshoot_costs(), usage);
    }
  }
  return sum;
}

double
Instance::calculate_costs(const ResVec & ressource_usage) const
{
  double sum = 0;
  for (unsigned int rid = 0; rid < resource_count(); rid++) {
    const Resource & resource = get_resource(rid);
    double usage =
        ressource_usage[rid] - resource.get_availability().get_flat_available();
    if (usage > 0) {
      sum += apply_polynomial(resource.get_investment_costs(), usage);
      sum += apply_polynomial(resource.get_overshoot_costs(), usage);
    }
  }
  return sum;
}

unsigned int
Instance::get_latest_deadline() const
{
  unsigned int ret = 0;
  for (unsigned int jid = 0; jid < this->job_count(); ++jid) {
    ret = std::max(ret, this->get_job(jid).get_deadline());
  }

  return ret;
}
