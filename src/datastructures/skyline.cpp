//
// Created by lukas on 06.12.17.
//

#include "skyline.hpp"
#include "../instance/instance.hpp" // for Instance
#include "../instance/job.hpp"      // for Job, Job::JobId
#include "../instance/resource.hpp" // for Resources, apply_polynomial
#include <assert.h>                 // for assert

namespace ds {

/* =============================================
 *         ArraySkyLine
 * =============================================
 */
template <bool support_it>
ArraySkyLineBase<support_it>::ArraySkyLineBase(const Instance * instance_in)
    : instance(instance_in), usage(instance->resource_count()),
      start_times(instance->job_count())
{
  unsigned int max_deadline = 0;
  for (unsigned int jid = 0; jid < this->instance->job_count(); ++jid) {
    const auto & job = this->instance->get_job(jid);
    max_deadline = std::max(max_deadline, job.get_deadline());
  }

  for (unsigned int rid = 0; rid < this->instance->resource_count(); ++rid) {
    this->usage[rid].resize(max_deadline, 0.0);
  }

  if constexpr (support_it) {
    this->events.resize(max_deadline);
  }
}

template <bool support_it>
void
ArraySkyLineBase<support_it>::remove_job(Job::JobId jid) noexcept
{
  const auto & job = this->instance->get_job(jid);
  this->remove_job(job);
}

template <bool support_it>
void
ArraySkyLineBase<support_it>::remove_job(const Job & job) noexcept
{
  for (unsigned int rid = 0; rid < this->instance->resource_count(); ++rid) {
    double u = job.get_resource_usage(rid);

    for (unsigned int s = this->start_times[job.get_jid()];
         s < this->start_times[job.get_jid()] + job.get_duration(); s++) {
      this->usage[rid][s] -= u;
    }
  }

  if constexpr (support_it) {
    this->events[this->start_times[job.get_jid()]].erase(
        std::pair<bool, Job::JobId>(true, job.get_jid()));
    this->events[this->start_times[job.get_jid() + job.get_duration()]].erase(
        std::pair<bool, Job::JobId>(false, job.get_jid()));
  }
}

template <bool support_it>
void
ArraySkyLineBase<support_it>::insert_job(Job::JobId jid,
                                         unsigned int pos) noexcept
{
  const auto & job = this->instance->get_job(jid);
  this->insert_job(job, pos);
}

template <bool support_it>
void
ArraySkyLineBase<support_it>::insert_job(const Job & job,
                                         unsigned int pos) noexcept
{
  this->start_times[job.get_jid()] = pos;

  for (unsigned int rid = 0; rid < this->instance->resource_count(); ++rid) {
    double u = job.get_resource_usage(rid);

    for (unsigned int s = pos; s < pos + job.get_duration(); s++) {
      this->usage[rid][s] += u;
    }
  }

  if constexpr (support_it) {
    this->events[pos].emplace(true, job.get_jid());
    this->events[pos + job.get_duration()].emplace(false, job.get_jid());
  }
}

template <bool support_it>
void
ArraySkyLineBase<support_it>::set_pos(const Job & job,
                                      unsigned int pos) noexcept
{
  unsigned int jid = job.get_jid();

  if (pos < this->start_times[jid]) {
    // Move left
    for (unsigned int rid = 0; rid < this->instance->resource_count(); ++rid) {
      double u = job.get_resource_usage(rid);

      // Add left
      for (unsigned int s = pos;
           s < std::min(pos + job.get_duration(), this->start_times[jid]);
           s++) {
	this->usage[rid][s] += u;
      }

      // Remove right
      for (unsigned int s =
               std::max(this->start_times[jid], pos + job.get_duration());
           s < this->start_times[jid] + job.get_duration(); s++) {
	this->usage[rid][s] -= u;
      }
    }
  }

  if constexpr (support_it) {
    this->events[this->start_times[job.get_jid()]].erase(
        std::pair<bool, Job::JobId>(true, job.get_jid()));
    this->events[this->start_times[job.get_jid()] + job.get_duration()].erase(
        std::pair<bool, Job::JobId>(false, job.get_jid()));

    this->events[pos].emplace(true, job.get_jid());
    this->events[pos + job.get_duration()].emplace(false, job.get_jid());
  }

  this->start_times[jid] = pos;
}

template <bool support_it>
void
ArraySkyLineBase<support_it>::set_pos(unsigned int jid,
                                      unsigned int pos) noexcept
{
  const Job & job = this->instance->get_job(jid);
  this->set_pos(job, pos);
}

template <bool support_it>
Resources
ArraySkyLineBase<support_it>::get_maximum(unsigned int l,
                                          unsigned int r) noexcept
{
  std::vector<double> max_usage(this->instance->resource_count(), 0.0);
  for (unsigned int rid = 0; rid < this->instance->resource_count(); ++rid) {
    for (unsigned int s = l; s < r; ++s) {
      max_usage[rid] = std::max(max_usage[rid], this->usage[rid][s]);
    }
  }

  return Resources(this->instance, std::move(max_usage));
}

template <bool support_it>
Resources
ArraySkyLineBase<support_it>::get_maximum() noexcept
{
  return this->get_maximum(0, (unsigned int)this->usage[0].size());
}

template <bool support_it>
MaxRange
ArraySkyLineBase<support_it>::get_maximum_range(unsigned int lb_in,
                                                unsigned int ub_in) const
    noexcept
{
  unsigned int lb = lb_in;
  unsigned int ub = lb_in;

  if (this->instance->resource_count() == 1) {
    double max_usage = 0.0;
    bool open = false;
    unsigned int t = lb_in;
    for (; t < ub_in; ++t) {
      if (!open) {
	if (this->usage[0][t] > max_usage) {
	  open = true;
	  lb = t;
	  max_usage = this->usage[0][t];
	}
      } else {
	if (this->usage[0][t] < max_usage) {
	  ub = t;
	  open = false;
	}
      }
    }
    if (t == ub_in) {
      ub = ub_in;
    }

    return std::pair<unsigned int, unsigned int>(lb, ub);
  } else {
    double max_costs = 0;
    bool open = false;

    unsigned int t = lb_in;
    for (; t < ub_in; ++t) {
      double costs = 0;
      for (unsigned int rid = 0; rid < this->instance->resource_count();
           ++rid) {
	costs += apply_polynomial(
	    this->instance->get_resource(rid).get_investment_costs(),
	    this->usage[rid][t]);
      }

      if (!open) {
	if (costs > max_costs) {
	  open = true;
	  lb = t;
	  max_costs = costs;
	}
      } else {
	if (costs < max_costs) {
	  ub = t;
	  open = false;
	}
      }
    }
    if (t == ub_in) {
      ub = ub_in;
    }

    return std::pair<unsigned int, unsigned int>(lb, ub);
  }
}

template <bool support_it>
MaxRange
ArraySkyLineBase<support_it>::get_maximum_range() const noexcept
{
  return this->get_maximum_range(0, (unsigned int)this->usage[0].size());
}

template <bool support_it>
typename ArraySkyLineBase<support_it>::iterator
ArraySkyLineBase<support_it>::begin() noexcept
{
  if constexpr (support_it) {
    return iterator(this, 0);
  } else {
    assert(false);
  }
}

template <bool support_it>
typename ArraySkyLineBase<support_it>::iterator
ArraySkyLineBase<support_it>::end() noexcept
{
  if constexpr (support_it) {
    return iterator(this, this->events.size());
  } else {
    assert(false);
  }
}

template <bool support_it>
typename ArraySkyLineBase<support_it>::iterator
ArraySkyLineBase<support_it>::lower_bound(unsigned int x) noexcept
{
  // Iterator automatically forwards to the first non-empty
  // time at which an event happens
  if constexpr (support_it) {
    return iterator(this, x);
  } else {
    (void)x;
    assert(false);
    return iterator(this, 0);
  }
}

template <bool support_it>
typename ArraySkyLineBase<support_it>::iterator
ArraySkyLineBase<support_it>::upper_bound(unsigned int x) noexcept
{
  if constexpr (support_it) {
    return iterator(this, (x + 1));
  } else {
    (void)x;
    assert(false);
    return iterator(this, 0);
  }
}

/*
template <bool support_it>
ArraySkyLineBase<support_it>::iterator::operator SkyLineIterator() noexcept
{
  return SkyLineIterator(*this);
}
*/

/* =============================================
 *         ArraySkyLineBase's iterator
 * =============================================
 */

template <bool support_it>
ArraySkyLineBase<support_it>::iterator::iterator() noexcept
    : sl(nullptr), time_index(0), event_it()
{}

template <bool support_it>
ArraySkyLineBase<support_it>::iterator::iterator(
    const iterator & other) noexcept
    : sl(other.sl), time_index(other.time_index), event_it(other.event_it),
      ev(other.ev)
{}

template <bool support_it>
ArraySkyLineBase<support_it>::iterator::iterator(iterator && other) noexcept
    : sl(std::move(other.sl)), time_index(other.time_index),
      event_it(other.event_it), ev(std::move(other.ev))
{}

template <bool support_it>
ArraySkyLineBase<support_it>::iterator::iterator(ArraySkyLineBase * sl_in,
                                                 size_t time_index_in) noexcept
    : sl(sl_in), time_index(time_index_in), event_it(), ev(sl_in->instance)
{
  if constexpr (support_it) {
    while ((time_index < this->sl->events.size()) &&
           (this->sl->events[time_index].empty())) {
      ++time_index;
    }
    if (time_index < this->sl->events.size()) {
      this->event_it = this->sl->events[time_index].begin();
    }

    // TODO actually, we need to reconstruct the resources at this point
    this->update_resource_amount(true);
    this->update_ev();
  } else {
    assert(false);
  }
}

template <bool support_it>
typename ArraySkyLineBase<support_it>::iterator &
ArraySkyLineBase<support_it>::iterator::
operator=(const iterator & other) noexcept
{
  this->sl = other.sl;
  this->time_index = other.time_index;
  this->event_it = other.event_it;
  this->ev = other.ev;

  return *this;
}

template <bool support_it>
typename ArraySkyLineBase<support_it>::iterator &
ArraySkyLineBase<support_it>::iterator::operator=(iterator && other) noexcept
{
  this->sl = std::move(other.sl);
  this->time_index = other.time_index;
  this->event_it = other.event_it;
  this->ev = std::move(other.ev);

  return *this;
}

template <bool support_it>
bool
ArraySkyLineBase<support_it>::iterator::operator==(const iterator & other) const
    noexcept
{
  if constexpr (support_it) {
    if (this->time_index >= this->sl->events.size()) {
      return (this->sl == other.sl) &&
             (other.time_index >= this->sl->events.size());
    } else {
      return (this->time_index == other.time_index) &&
             (this->event_it == other.event_it) && (this->sl == other.sl);
    }
  } else {
    assert(false);
    return false;
  }
}

template <bool support_it>
bool
ArraySkyLineBase<support_it>::iterator::operator!=(const iterator & other) const
    noexcept
{
  return !(*this == other);
}

template <bool support_it>
typename ArraySkyLineBase<support_it>::iterator &
ArraySkyLineBase<support_it>::iterator::operator++() noexcept
{
  if constexpr (support_it) {
    this->event_it++;
    if (this->event_it == this->sl->events[this->time_index].end()) {
      this->time_index++;
      while ((this->time_index < this->sl->events.size()) &&
             (this->sl->events[this->time_index].empty())) {
	this->time_index++;
      }
      if (this->time_index < this->sl->events.size()) {
	this->event_it = this->sl->events[this->time_index].begin();
      }
    }

    this->update_resource_amount(true);
    this->update_ev();
    return *this;
  } else {
    assert(false);
  }
}

template <bool support_it>
typename ArraySkyLineBase<support_it>::iterator
ArraySkyLineBase<support_it>::iterator::operator++(int) noexcept
{
  iterator cpy = *this;

  ++(*this);

  return cpy;
}

template <bool support_it>
typename ArraySkyLineBase<support_it>::iterator &
ArraySkyLineBase<support_it>::iterator::operator--() noexcept
{
  if constexpr (support_it) {
    if (this->event_it != this->sl->events[this->time_index].begin()) {
      this->event_it--;
    } else {
      this->time_index--;
      while (this->sl->events[this->time_index].empty()) {
	this->time_index--;
      }
      this->event_it = this->sl->events[this->time_index].end() - 1;
    }

    this->update_resource_amount(false);
    this->update_ev();

    return *this;
  } else {
    assert(false);
  }
}

template <bool support_it>
typename ArraySkyLineBase<support_it>::iterator
ArraySkyLineBase<support_it>::iterator::operator--(int) noexcept
{
  iterator cpy = *this;

  --(*this);

  return cpy;
}

template <bool support_it>
typename ArraySkyLineBase<support_it>::iterator &
ArraySkyLineBase<support_it>::iterator::operator+=(size_t steps) noexcept
{
  if constexpr (support_it) {
    for (unsigned int i = 0; i < steps; ++i) {
      this->event_it++;
      if (this->event_it == this->sl->events[this->time_index].end()) {
	this->time_index++;
	while ((this->time_index < this->sl->events.size()) &&
	       (this->sl->events[this->time_index].empty())) {
	  this->time_index++;
	}
	if (this->time_index < this->sl->events.size()) {
	  this->event_it = this->sl->events[this->time_index].begin();
	}
      }
      this->update_resource_amount(true);
    }

    this->update_ev();

    return *this;
  } else {
    assert(false);
  }
}

template <bool support_it>
typename ArraySkyLineBase<support_it>::iterator &
ArraySkyLineBase<support_it>::iterator::operator-=(size_t steps) noexcept
{
  if constexpr (support_it) {
    for (size_t i = 0; i < steps; ++i) {
      if (this->event_it != this->sl->events[this->time_index].begin()) {
	this->event_it--;
      } else {
	this->time_index--;
	while (this->sl->events[this->time_index].empty()) {
	  this->time_index--;
	}
	this->event_it = this->sl->events[this->time_index].end() - 1;
      }
      this->update_resource_amount(false);
    }
    this->update_ev();

    return *this;
  } else {
    assert(false);
  }
}

template <bool support_it>
typename ArraySkyLineBase<support_it>::iterator
ArraySkyLineBase<support_it>::iterator::operator+(size_t steps) const noexcept
{
  iterator cpy = *this;
  cpy += steps;
  return cpy;
}

template <bool support_it>
typename ArraySkyLineBase<support_it>::iterator
ArraySkyLineBase<support_it>::iterator::operator-(size_t steps) const noexcept
{
  iterator cpy = *this;
  cpy -= steps;
  return cpy;
}

template <bool support_it>
const SkyLineEvent & ArraySkyLineBase<support_it>::iterator::operator*() const
    noexcept
{
  return this->ev;
}

template <bool support_it>
const SkyLineEvent * ArraySkyLineBase<support_it>::iterator::operator->() const
    noexcept
{
  return &(this->ev);
}

template <bool support_it>
void
ArraySkyLineBase<support_it>::iterator::update_resource_amount(
    bool went_forward) noexcept
{
  if constexpr (support_it) {
    if (this->time_index >= this->sl->events.size()) {
      return;
    }

    auto [start, job_id] = *(this->event_it);
    const Job & job = this->sl->instance->get_job(job_id);

    if (went_forward) {
      if (start) {
	for (unsigned int rid = 0; rid < this->sl->instance->resource_count();
	     ++rid) {
	  this->ev.usage.getUsage()[rid] += job.get_resource_usage(rid);
	}
      } else {
	for (unsigned int rid = 0; rid < this->sl->instance->resource_count();
	     ++rid) {
	  this->ev.usage.getUsage()[rid] -= job.get_resource_usage(rid);
	}
      }
    } else {
      if (start) {
	for (unsigned int rid = 0; rid < this->sl->instance->resource_count();
	     ++rid) {
	  this->ev.usage.getUsage()[rid] -= job.get_resource_usage(rid);
	}
      } else {
	for (unsigned int rid = 0; rid < this->sl->instance->resource_count();
	     ++rid) {
	  this->ev.usage.getUsage()[rid] += job.get_resource_usage(rid);
	}
      }
    }
  } else {
    (void)went_forward;
    assert(false);
  }
}

template <bool support_it>
void
ArraySkyLineBase<support_it>::iterator::update_ev() noexcept
{
  if constexpr (support_it) {
    if (this->time_index >= this->sl->events.size()) {
      return;
    }
    bool start = this->event_it->first;
    this->ev.start = start;
    this->ev.where = (unsigned int)this->time_index;
  } else {
    assert(false);
  }
}

/* =============================================
 *         TreeSkyLine
 * =============================================
 */

template <bool ranged, bool single_resource>
unsigned int
TreeSkyLineBase<ranged, single_resource>::JobNodeTraits::get_lower(
    const Node & job)
{
  return job.start;
}

template <bool ranged, bool single_resource>
unsigned int
TreeSkyLineBase<ranged, single_resource>::JobNodeTraits::get_upper(
    const Node & job)
{
  return job.start + job.length;
}

template <bool ranged, bool single_resource>
const typename TreeSkyLineBase<ranged, single_resource>::ValueType &
TreeSkyLineBase<ranged, single_resource>::JobNodeTraits::get_value(
    const Node & job)
{
  // TODO if we templatized the job class, then we could just return
  // .get_usage(RID) here…
  return job.usage;
}

template <bool ranged, bool single_resource>
TreeSkyLineBase<ranged, single_resource>::TreeSkyLineBase(
    const Instance * instance_in)
    : instance(instance_in), nodes(this->instance->job_count())
{
  for (size_t jid = 0; jid < this->instance->job_count(); ++jid) {
    if constexpr (!single_resource) {
      nodes[jid].usage = Resources{
          this->instance,
          this->instance->get_job((unsigned int)jid).get_resource_usage()};
    } else {
      nodes[jid].usage =
          this->instance->get_job((unsigned int)jid).get_resource_usage()[0];
    }

    nodes[jid].start = 0;
    nodes[jid].length =
        this->instance->get_job((unsigned int)jid).get_duration();
  }
}

template <bool ranged, bool single_resource>
void
TreeSkyLineBase<ranged, single_resource>::set_pos(const Job & job,
                                                  unsigned int pos) noexcept
{
  this->set_pos(job.get_jid(), pos);
}

template <bool ranged, bool single_resource>
void
TreeSkyLineBase<ranged, single_resource>::set_pos(Job::JobId jid,
                                                  unsigned int pos) noexcept
{
  this->remove_job(jid);
  this->insert_job(jid, pos);
}

template <bool ranged, bool single_resource>
void
TreeSkyLineBase<ranged, single_resource>::remove_job(Job::JobId jid) noexcept
{
  Node & n = this->nodes[jid];
  this->t.remove(n);
}

template <bool ranged, bool single_resource>
void
TreeSkyLineBase<ranged, single_resource>::remove_job(const Job & job) noexcept
{
  this->remove_job(job.get_jid());
}

template <bool ranged, bool single_resource>
void
TreeSkyLineBase<ranged, single_resource>::insert_job(Job::JobId jid,
                                                     unsigned int pos) noexcept
{
  Node & n = this->nodes[jid];
  n.start = pos;
  this->t.insert(n);
}

template <bool ranged, bool single_resource>
void
TreeSkyLineBase<ranged, single_resource>::insert_job(const Job & job,
                                                     unsigned int pos) noexcept
{
  this->insert_job(job.get_jid(), pos);
}

template <bool ranged, bool single_resource>
typename TreeSkyLineBase<ranged, single_resource>::MaxRange
TreeSkyLineBase<ranged, single_resource>::get_maximum_range() const noexcept
{
  if constexpr (ranged) {
    // Always true - otherwise, its undefined behavior to call this method
    auto combiner = this->t.template get_combiner<MaxCombiner>();
    return {combiner.get_left_border(), combiner.get_right_border()};
  } else {
    assert(false);
    return {0, 0};
  }
}

template <bool ranged, bool single_resource>
typename TreeSkyLineBase<ranged, single_resource>::MaxRange
TreeSkyLineBase<ranged, single_resource>::get_maximum_range(
    unsigned int l, unsigned int r) const noexcept
{
  if constexpr (ranged) {
    // Always true - otherwise, its undefined behavior to call this method
    auto combiner = this->t.template get_combiner<MaxCombiner>(l, r);
    return {combiner.get_left_border(), combiner.get_right_border()};
  } else {
    (void)l;
    (void)r;
    assert(false);
    return {0, 0};
  }
}

template <bool ranged, bool single_resource>
typename TreeSkyLineBase<ranged, single_resource>::ValueType
TreeSkyLineBase<ranged, single_resource>::get_maximum()
{
  return this->t.template get_combined<MaxCombiner>();
}

template <bool ranged, bool single_resource>
typename TreeSkyLineBase<ranged, single_resource>::ValueType
TreeSkyLineBase<ranged, single_resource>::get_maximum(unsigned int l,
                                                      unsigned int r)
{
  return this->t.template get_combined<MaxCombiner>(l, r);
}

template <bool ranged, bool single_resource>
TreeSkyLineBase<ranged, single_resource>::iterator::iterator()
    : tree(nullptr), sub_it(), ev{}
{}

template <bool ranged, bool single_resource>
TreeSkyLineBase<ranged, single_resource>::iterator::iterator(
    const TreeSkyLineBase<ranged, single_resource>::iterator & other)
    : tree(other.tree), sub_it(other.sub_it), ev(other.ev)
{}

template <bool ranged, bool single_resource>
TreeSkyLineBase<ranged, single_resource>::iterator::iterator(
    TreeSkyLineBase<ranged, single_resource>::iterator && other)
    : tree(other.tree), sub_it(std::move(other.sub_it)), ev(std::move(other.ev))
{}

template <bool ranged, bool single_resource>
TreeSkyLineBase<ranged, single_resource>::iterator::iterator(
    ds::TreeSkyLineBase<ranged, single_resource>::Tree * t_in,
    const Instance * instance,
    ds::TreeSkyLineBase<ranged, single_resource>::iterator::SubIterator
        sub_it_in)
    : tree(t_in), sub_it(sub_it_in), ev(instance)
{
  if (sub_it_in == tree->begin()) {
    this->update_resource_amount(true);
  } else if (sub_it_in == tree->end()) {
    this->ev.usage = Resources();
  } else {
    this->ev.usage = tree->query(sub_it_in->get_point());
  }

  this->update_event();
}

template <bool ranged, bool single_resource>
typename TreeSkyLineBase<ranged, single_resource>::iterator &
TreeSkyLineBase<ranged, single_resource>::iterator::
operator=(const TreeSkyLineBase<ranged, single_resource>::iterator & other)
{
  this->tree = other.tree;
  this->sub_it = other.sub_it;
  this->ev = other.ev;

  return *this;
}

template <bool ranged, bool single_resource>
typename TreeSkyLineBase<ranged, single_resource>::iterator &
TreeSkyLineBase<ranged, single_resource>::iterator::
operator=(TreeSkyLineBase<ranged, single_resource>::iterator && other)
{
  this->tree = other.tree;
  this->sub_it = std::move(other.sub_it);
  this->ev = std::move(other.ev);

  return *this;
}

template <bool ranged, bool single_resource>
bool
TreeSkyLineBase<ranged, single_resource>::iterator::
operator==(const iterator & other) const
{
  return (this->sub_it == other.sub_it);
}

template <bool ranged, bool single_resource>
bool
TreeSkyLineBase<ranged, single_resource>::iterator::
operator!=(const iterator & other) const
{
  return !(*this == other);
}

template <bool ranged, bool single_resource>
void
TreeSkyLineBase<ranged, single_resource>::iterator::update_resource_amount(
    bool went_forward) noexcept
{
  if (this->sub_it == this->tree->end()) {
    return;
  }

  if (went_forward) {
    if (this->sub_it->is_start()) {
      this->ev.usage +=
          (static_cast<const Node *>(this->sub_it->get_interval()))->usage;
    } else {
      this->ev.usage -=
          (static_cast<const Node *>(this->sub_it->get_interval()))->usage;
    }
  } else {
    if (this->sub_it->is_start()) {
      this->ev.usage -=
          (static_cast<const Node *>(this->sub_it->get_interval()))->usage;
    } else {
      this->ev.usage +=
          (static_cast<const Node *>(this->sub_it->get_interval()))->usage;
    }
  }
}

template <bool ranged, bool single_resource>
void
TreeSkyLineBase<ranged, single_resource>::iterator::update_event() noexcept
{
  if (this->sub_it != this->tree->end()) {
    this->ev_node = static_cast<const Node *>(this->sub_it->get_interval());
    this->ev.start = this->sub_it->is_start();
    if (this->ev.start) {
      this->ev.where = this->ev_node->start;
    } else {
      this->ev.where = this->ev_node->start + this->ev_node->length;
    }
  }
}

template <bool ranged, bool single_resource>
typename TreeSkyLineBase<ranged, single_resource>::iterator &
TreeSkyLineBase<ranged, single_resource>::iterator::operator++()
{
  this->sub_it++;
  this->update_resource_amount(true);
  this->update_event();

  return *this;
}

template <bool ranged, bool single_resource>
typename TreeSkyLineBase<ranged, single_resource>::iterator
TreeSkyLineBase<ranged, single_resource>::iterator::operator++(int)
{
  auto copy = *this;

  this->sub_it++;
  this->update_resource_amount(true);
  this->update_event();

  return copy;
}

template <bool ranged, bool single_resource>
typename TreeSkyLineBase<ranged, single_resource>::iterator &
TreeSkyLineBase<ranged, single_resource>::iterator::operator+=(size_t steps)
{
  for (size_t i = 0; i < steps; ++i) {
    this->sub_it++;
    this->update_resource_amount(true);
  }
  this->update_event();

  return *this;
}

template <bool ranged, bool single_resource>
typename TreeSkyLineBase<ranged, single_resource>::iterator
TreeSkyLineBase<ranged, single_resource>::iterator::
operator+(size_t steps) const
{
  auto copy = *this;
  copy += steps;
  return copy;
}

template <bool ranged, bool single_resource>
typename TreeSkyLineBase<ranged, single_resource>::iterator &
TreeSkyLineBase<ranged, single_resource>::iterator::operator--()
{
  this->sub_it--;
  this->update_resource_amount(false);
  this->update_event();

  return *this;
}

template <bool ranged, bool single_resource>
typename TreeSkyLineBase<ranged, single_resource>::iterator
TreeSkyLineBase<ranged, single_resource>::iterator::operator--(int)
{
  auto copy = *this;

  this->sub_it--;
  this->update_resource_amount(false);
  this->update_event();

  return copy;
}

template <bool ranged, bool single_resource>
typename TreeSkyLineBase<ranged, single_resource>::iterator &
TreeSkyLineBase<ranged, single_resource>::iterator::operator-=(size_t steps)
{
  for (size_t i = 0; i < steps; ++i) {
    this->sub_it--;
    this->update_resource_amount(false);
  }
  this->update_event();

  return *this;
}

template <bool ranged, bool single_resource>
typename TreeSkyLineBase<ranged, single_resource>::iterator
TreeSkyLineBase<ranged, single_resource>::iterator::
operator-(size_t steps) const
{
  auto copy = *this;
  copy -= steps;
  return copy;
}

template <bool ranged, bool single_resource>
typename TreeSkyLineBase<ranged, single_resource>::iterator::reference
    TreeSkyLineBase<ranged, single_resource>::iterator::operator*() const
{
  return this->ev;
}

template <bool ranged, bool single_resource>
typename TreeSkyLineBase<ranged, single_resource>::iterator::pointer
    TreeSkyLineBase<ranged, single_resource>::iterator::operator->() const
{
  return &this->ev;
}

template <bool ranged, bool single_resource>
typename TreeSkyLineBase<ranged, single_resource>::iterator
TreeSkyLineBase<ranged, single_resource>::begin()
{
  return iterator(&this->t, this->instance, this->t.begin());
}

template <bool ranged, bool single_resource>
typename TreeSkyLineBase<ranged, single_resource>::iterator
TreeSkyLineBase<ranged, single_resource>::end()
{
  return iterator(&this->t, this->instance, this->t.end());
}

template <bool ranged, bool single_resource>
typename TreeSkyLineBase<ranged, single_resource>::iterator
TreeSkyLineBase<ranged, single_resource>::lower_bound(unsigned int x)
{
  return iterator(&this->t, this->instance, this->t.lower_bound_event(x));
}

template <bool ranged, bool single_resource>
typename TreeSkyLineBase<ranged, single_resource>::iterator
TreeSkyLineBase<ranged, single_resource>::upper_bound(unsigned int x)
{
  return iterator(&this->t, this->instance, this->t.upper_bound_event(x));
}

/*
template <bool ranged, bool single_resource>
TreeSkyLineBase<ranged, single_resource>::iterator::
operator SkyLineIterator() noexcept
{
  return SkyLineIterator(*(TreeSkyLineBase<ranged,
single_resource>::iterator*)this);
}
*/

template class TreeSkyLineBase<true, false>;
template class TreeSkyLineBase<false, false>;
template class TreeSkyLineBase<true, true>;
template class TreeSkyLineBase<false, true>;

template class ArraySkyLineBase<true>;
template class ArraySkyLineBase<false>;
} // namespace ds
