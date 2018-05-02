//
// Created by lukas on 06.12.17.
//

#include "skyline.hpp"

namespace ds {

template<class ValueT>
bool
RangedMaxCombiner<ValueT>::combine_with(ValueT a, ValueT edge_val)
{
	this->val = std::max(this->val, a + edge_val);
	return false;
}

template<class ValueT>
ValueT
RangedMaxCombiner<ValueT>::get()
{
	return this->val;
}

template<class ValueT>
bool
RangedMaxCombiner<ValueT>::rebuild(ValueT a, ValueT a_edge_val, ValueT b, ValueT b_edge_val)
{
	auto old_val = this->val;
	this->val = std::max(a + a_edge_val, b + b_edge_val);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
	return old_val != val;
#pragma GCC diagnostic pop
}

template<class ValueT>
RangedMaxCombiner<ValueT>::RangedMaxCombiner(ValueT val_in)
				: val(val_in)
{}

unsigned int
SkyLine::JobNodeTraits::get_lower(const Node &job)
{
	return job.start;
}

unsigned int
SkyLine::JobNodeTraits::get_upper(const Node &job)
{
	return job.start + job.length;
}

double
SkyLine::JobNodeTraits::get_value(const Node &job)
{
	// TODO if we templatized the job class, then we could just return .get_usage(RID) here…
	return job.usage;
}

SkyLine::SkyLine(const Instance * instance_in, unsigned int rid_in)
	: rid(rid_in), instance(instance_in), nodes(instance_in->job_count())
{
	for (size_t jid = 0 ; jid < this->instance->job_count() ; ++jid) {
		nodes[jid].usage = this->instance->get_job((unsigned int)jid).get_resource_usage(this->rid);
		nodes[jid].start = 0;
		nodes[jid].length = this->instance->get_job((unsigned int)jid).get_duration();
		this->t.insert(nodes[jid]);
	}
}

void
SkyLine::set_pos(const Job & job, unsigned int pos)
{
	Node & n = this->nodes[job.get_jid()];
	this->t.remove(n);
	n.start = pos;
	this->t.insert(n);
}

double
SkyLine::get_maximum()
{
	return this->t.get_combined<MaxCombiner>();
}

} // namespace ds