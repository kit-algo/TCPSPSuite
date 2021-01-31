#include "job.hpp"

#include "resource.hpp" // for ResVec

#include <boost/container/vector.hpp> // for vector
#include <limits>                     // for numeric_limits

// TODO move usages in?
Job::Job(unsigned int release_in, unsigned int deadline_in,
         unsigned int duration_in, ResVec usages_in, unsigned int id)
    : resource_usage(usages_in), duration(duration_in), release(release_in),
      deadline(deadline_in)
{
	this->jid = id;
}

Job::Job() : jid(std::numeric_limits<decltype(this->jid)>::max()) {}

Job
Job::clone() const
{
	ResVec copied_usages = this->resource_usage;
	Job cloned(this->release, this->deadline, this->duration, copied_usages,
	           this->jid);
	cloned.set_hint(this->hint);
	return cloned;
}

Job::Job(unsigned int release_in, unsigned int deadline_in, const Job & other)
    : Job(other)
{
	this->release = release_in;
	this->deadline = deadline_in;
}

unsigned int
Job::get_duration() const
{
	return this->duration;
}

Maybe<unsigned int>
Job::get_hint() const
{
	return this->hint;
}

void
Job::set_hint(Maybe<unsigned int> hint_in)
{
	this->hint = hint_in;
}

double
Job::get_resource_usage(unsigned int rid) const
{
	return this->resource_usage[rid];
}

const ResVec &
Job::get_resource_usage() const
{
	return this->resource_usage;
}

unsigned int
Job::get_jid() const
{
	return this->jid;
}

void
Job::set_id(unsigned int id)
{
	this->jid = id;
}

unsigned int
Job::get_release() const
{
	return this->release;
}

unsigned int
Job::get_deadline() const
{
	return this->deadline;
}
