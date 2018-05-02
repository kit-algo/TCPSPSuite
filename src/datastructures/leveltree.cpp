#include "leveltree.hpp"
#include <algorithm>                             // for min, binary_search
#include <functional>                            // for function
#include <limits>                                // for numeric_limits
#include <string>                                // for string
#include <utility>                               // for make_pair
#include "../datastructures/jobset.hpp"             // for JobSet
#include "../datastructures/maybe.hpp"      // for Maybe
#include "../util/intervalrunner.hpp"               // for process_intervals
#include "../util/log.hpp"                          // for Log

namespace level_tree {

LinearLevelAssigner::LinearLevelAssigner(double min, double max, unsigned int levels_in)
  : min_val(min), level_size((max-min)/levels_in), levels(levels_in)
{}

unsigned int
LinearLevelAssigner::get_level(double amount) const
{
  return std::min((unsigned int)((amount - min_val) / level_size),
				          this->levels - 1);
}

bool
LinearLevelAssigner::operator==(const LinearLevelAssigner & rhs) const
{
  return ((rhs.min_val >= this->min_val - DOUBLE_DELTA) &&
          (rhs.min_val <= this->min_val + DOUBLE_DELTA) &&
          (rhs.level_size >= this->level_size - DOUBLE_DELTA) &&
          (rhs.level_size <= this->level_size + DOUBLE_DELTA) &&
          (rhs.levels == this->levels));
}

unsigned int
LinearLevelAssigner::level_count() const
{
	return this->levels;
}

} // namespace level_tree

template<class Key, class LevelAssigner>
const std::vector<typename LevelTree<Key, LevelAssigner>::Interval> &
LevelTree<Key, LevelAssigner>::Intervals::get() const
{
  return this->content;
}

template<class Key, class LevelAssigner>
Key
LevelTree<Key, LevelAssigner>::Intervals::coverage() const
{
  // TODO works only with right-open or left-open!
  // … or does it? lower() and upper() should *bound*…
  Key sum = 0;
  for (const auto & i : this->content) {
    sum += (i.upper() - i.lower());
  }

  return sum;
}

template<class Key, class LevelAssigner>
bool
LevelTree<Key, LevelAssigner>::Intervals::overlaps(const Interval & interval) const
{
	return std::binary_search(this->content.begin(), this->content.end(), interval,
	                          [](const Interval & lhs, const Interval & rhs) {
		                          return lhs.upper() <= rhs.lower(); // TODO works only for half-open
	                          });
}

template<class Key, class LevelAssigner>
bool
LevelTree<Key, LevelAssigner>::Intervals::empty() const
{
  return this->content.empty();
}

template<class Key, class LevelAssigner>
void
LevelTree<Key, LevelAssigner>::Intervals::push_back(Interval interval)
{
  return this->content.push_back(interval);
}

template<class Key, class LevelAssigner>
typename LevelTree<Key, LevelAssigner>::Intervals
LevelTree<Key, LevelAssigner>::Intervals::operator-(const Intervals & rhs) const
{
  return this->operator_base([](bool a_open, bool b_open) {
    return a_open && (!b_open);
  }, rhs);
}

template<class Key, class LevelAssigner>
typename LevelTree<Key, LevelAssigner>::Intervals
LevelTree<Key, LevelAssigner>::Intervals::operator+(const Intervals & rhs) const
{
  return this->operator_base([](bool a_open, bool b_open) {
    return a_open || b_open;
  }, rhs);
}

template<class Key, class LevelAssigner>
typename LevelTree<Key, LevelAssigner>::Intervals
LevelTree<Key, LevelAssigner>::Intervals::operator&(const Intervals & rhs) const
{
  return this->operator_base([](bool a_open, bool b_open) {
    return a_open && b_open;
  }, rhs);
}

template<class Key, class LevelAssigner>
typename LevelTree<Key, LevelAssigner>::Intervals
LevelTree<Key, LevelAssigner>::Intervals::operator!() const
{
  Intervals negative;
  negative.push_back(make_interval(std::numeric_limits<Key>::min(), std::numeric_limits<Key>::max()));

  return negative - (*this);
}

template<class Key, class LevelAssigner>
typename LevelTree<Key, LevelAssigner>::Intervals
LevelTree<Key, LevelAssigner>::Intervals::move_left(Key amount) const
{
  Intervals result;

  for (const auto & i : this->content) {
    // TODO works only with semi-open!
    if (amount >= i.upper()) {
      continue; // completely shifted out
    }
    auto upper = i.upper() - amount;

    auto lower = i.lower() - amount;
    if (amount > i.lower()) {
      lower = std::numeric_limits<Key>::min();
    }

    result.push_back(make_interval(lower, upper));
  }

  return result;
}

template<class Key, class LevelAssigner>
typename LevelTree<Key, LevelAssigner>::Intervals
LevelTree<Key, LevelAssigner>::Intervals::extend(Key extend_left, Key extend_right) const
{
  Intervals result;

  std::vector<std::pair<Key, bool>> events;
  for (const auto & interval : this->content) {
    // TODO FIXME works only if 0 is the minimum value
    if (interval.lower() < extend_left) {
      events.push_back({0, true});
    } else {
      events.push_back({interval.lower() - extend_left, true});
    }

    if ((std::numeric_limits<Key>::max() - interval.upper()) < extend_right) {
      events.push_back({std::numeric_limits<Key>::max(), false});
    } else {
      events.push_back({interval.upper() + extend_right, false});
    }
  }

  std::sort(events.begin(), events.end(), [](const auto & lhs, const auto & rhs) {
    if (lhs.first < rhs.first) {
      return true;
    } else if (lhs.first > rhs.first) {
      return false;
    } else {
      if (lhs.second && !rhs.second) {
        return true;
      } else {
        return false;
      }
    }
  });

  unsigned int counter = 0;
  Key started = std::numeric_limits<Key>::max();

  for (const auto & event : events) {
    if (event.second) {
      counter++;
      if (counter == 1) {
        started = event.first;
      }
    } else {
      counter--;
#ifdef ENABLE_ASSERTIONS
      assert(started != std::numeric_limits<Key>::max());
#endif
      if (counter == 0) {
        result.push_back(make_interval(started, event.first));
      }
    }
  }

  return result;
}

template<class Key, class LevelAssigner>
template<class binop>
typename LevelTree<Key, LevelAssigner>::Intervals
LevelTree<Key, LevelAssigner>::Intervals::operator_base(binop op, const Intervals &rhs) const
{
  Intervals result;
  Maybe<Key> open_since;
  bool a_open = false;
  bool b_open = false;

  std::function<bool(typename std::vector<Interval>::const_iterator)> data_getter = [&](typename std::vector<Interval>::const_iterator it) {
    (void)it;
    return false; // dummy value
  };

  std::function<Key(typename std::vector<Interval>::const_iterator, bool)> point_getter = [&](typename std::vector<Interval>::const_iterator it, bool lower) {
    if (lower) {
      return it->lower();
    } else {
      return it->upper();
    }
  };

  std::function<void(Key, Maybe<bool>, Maybe<bool>, Maybe<bool>, Maybe<bool>)> handler = [&](Key point, Maybe<bool> a_start, Maybe<bool> a_end, Maybe<bool> b_start, Maybe<bool> b_end) {
    if (a_start.valid()) {
      a_open = true;
    } else if (a_end.valid()) {
      a_open = false;
    }

    if (b_start.valid()) {
      b_open = true;
    } else if (b_end.valid()) {
      b_open = false;
    }

    if (open_since.valid()) {
      if (! op(a_open, b_open)) {
        result.push_back(make_interval(open_since.value(), point));
        open_since = Maybe<Key>();
      }
    } else {
      if (op(a_open, b_open)) {
        open_since = Maybe<Key>(point);
      }
    }
  };

  // TODO move to const iterators?
  process_intervals<decltype(this->content.begin()), decltype(rhs.content.begin()), Key, decltype(point_getter), decltype(handler), decltype(data_getter), bool>(this->content.begin(), this->content.end(), rhs.content.begin(), rhs.content.end(), point_getter, handler, data_getter);

  return result;
}

template<class Key, class LevelAssigner>
void
LevelTree<Key, LevelAssigner>::insert(const Interval &interval, JobSet payload) {
  LevelProfile old_levels = this->get_profile(interval);
  this->content.add(std::make_pair(interval, payload));
  LevelProfile new_levels = this->get_profile(interval);

  this->update_levels(old_levels, new_levels);
}

template<class Key, class LevelAssigner>
void
LevelTree<Key, LevelAssigner>::remove(const Interval &interval, const JobSet &payload) {
  LevelProfile old_levels = this->get_profile(interval);
  // TODO propagate reference here?
  this->content.subtract(std::make_pair(interval, payload));
  LevelProfile new_levels = this->get_profile(interval);

  this->update_levels(old_levels, new_levels);
}

template<class Key, class LevelAssigner>
void
LevelTree<Key, LevelAssigner>::insert(const Interval &interval, JobId job, double amount) {
  BOOST_LOG(l.d(4)) << "Inserting: " << interval << " Job: " << job << " amount: " << amount ;

  LevelProfile old_levels = this->get_profile(interval);
  JobSet payload {job, amount};
  this->content.add(std::make_pair(interval, payload));
  LevelProfile new_levels = this->get_profile(interval);
  //BOOST_LOG(l.d(4)) << "Old level profile: " << old_levels ;
  //BOOST_LOG(l.d(4)) << "New level profile: " << new_levels ;

  this->update_levels(old_levels, new_levels);
  //BOOST_LOG(l.d(4)) << "New level 0: " << this->get_level(0) ;
  //BOOST_LOG(l.d(4)) << "New level 1: " << this->get_level(1) ;
}

template<class Key, class LevelAssigner>
void
LevelTree<Key, LevelAssigner>::remove(const Interval &interval, JobId job, double amount) {
  LevelProfile old_levels = this->get_profile(interval);
  JobSet payload {job, amount};
  BOOST_LOG(l.d(5)) << "Subtracting from " << interval << " payload " << payload ;
  this->content.subtract(std::make_pair(interval, payload));
  LevelProfile new_levels = this->get_profile(interval);

  this->update_levels(old_levels, new_levels);
}

/*
template<class Key, class LevelAssigner>
void
LevelTree<Key, LevelAssigner>::move(const Interval &old_interval, const Interval &new_interval, JobId job, double amount) {
  // TODO FIXME levels are updated twice!
  this->remove(old_interval, job, amount);
  this->insert(new_interval, job, amount);
}
*/

template<class Key, class LevelAssigner>
typename LevelTree<Key, LevelAssigner>::Intervals
LevelTree<Key, LevelAssigner>::get_level(unsigned int level)
{
#ifdef ENABLE_ASSERTIONS
  assert(level <= this->level_assigner.level_count());
#endif

  // TODO warum kopieren wir hier alles?
  Intervals intervals;
  for (auto entry : this->levels[level]) {
    intervals.push_back(entry);
  }

  return intervals;
}

template<class Key, class LevelAssigner>
LevelTree<Key, LevelAssigner>::LevelTree(LevelAssigner level_assigner_in, Interval horizon_in)
  : levels(level_assigner_in.level_count()),
    level_assigner(level_assigner_in), horizon(horizon_in), l("LEVELTREE")
{
  this->levels[0].add(horizon_in);
}

template<class Key, class LevelAssigner>
typename LevelTree<Key, LevelAssigner>::LevelProfile
LevelTree<Key, LevelAssigner>::get_profile()
{
  return this->get_profile(this->horizon);
}

template<class Key, class LevelAssigner>
typename LevelTree<Key, LevelAssigner>::LevelProfile
LevelTree<Key, LevelAssigner>::get_profile(const Interval &query)
{
  LevelProfile res;

  auto it = this->content.find(query);
  while ((it != this->content.end()) && (! strictly_after(it->first, query))) {
    if (contains(query, it->first)) {
      res.push_back(std::tuple<const Interval, unsigned int, double>(
              it->first,
              this->level_assigner.get_level(it->second.get_amount()),
              it->second.get_amount()));
    } else {
      Interval interval = make_interval(std::max(it->first.lower(), query.lower()), std::min(it->first.upper(), query.upper()));
      res.push_back(std::tuple<const Interval, unsigned int, double>(
              interval,
              this->level_assigner.get_level(it->second.get_amount()),
              it->second.get_amount()));
    }
    it++;
  }

  return res;
}

template<class Key, class LevelAssigner>
typename LevelTree<Key, LevelAssigner>::ContentData
LevelTree<Key, LevelAssigner>::get_content()
{
  ContentData res;

  for (auto entry : this->content) {
    res.push_back(std::make_pair(entry.first, entry.second));
  }

  return res;
}

template<class Key, class LevelAssigner>
void
LevelTree<Key, LevelAssigner>::update_levels(const LevelProfile &oldp, const LevelProfile &newp)
{
  std::function<unsigned int(const typename LevelProfile::const_iterator &)> data_getter = [&](const typename LevelProfile::const_iterator & it) {
    return std::get<1>(*it);
  };

  std::function<Key(const typename LevelProfile::const_iterator &, bool)> point_getter = [&](const typename LevelProfile::const_iterator & it, bool lower) {
    if (lower) {
      return std::get<0>(*it).lower();
    } else {
      return std::get<0>(*it).upper();
    }
  };

  std::vector<std::tuple<Interval, unsigned int, unsigned int>> changes;
  unsigned int old_level = 0, new_level = 0;
  Key last_point = 0;

  std::function<void(Key, Maybe<unsigned int>, Maybe<unsigned int>, Maybe<unsigned int>, Maybe<unsigned int>)> handler = [&](Key point, Maybe<unsigned int> old_start, Maybe<unsigned int> old_end, Maybe<unsigned int> new_start, Maybe<unsigned int> new_end) {
    if (old_level != new_level) {
      changes.push_back(std::make_tuple(this->make_interval(last_point, point),
                                        old_level, new_level));
    }

    if (old_end.valid()) {
      old_level = 0;
    }
    if (old_start.valid()) {
      old_level = old_start.value();
    }

    if (new_end.valid()) {
      new_level = 0;
    }
    if (new_start.valid()) {
      new_level = new_start.value();
    }

    last_point = point;
  };

  auto old_start_it = oldp.begin();
  auto old_end_it = oldp.end();
  auto new_start_it = newp.begin();
  auto new_end_it = newp.end();

  // Call the template
  process_intervals<decltype(old_start_it), decltype(new_start_it), Key, decltype(point_getter), decltype(handler), decltype(data_getter), unsigned int>(old_start_it, old_end_it, new_start_it, new_end_it, point_getter, handler, data_getter);

  //BOOST_LOG(l.d(5)) << "Update Changes: " << changes ;

  // Update the sets
  for (const auto & change : changes) {
    const Interval & i = std::get<0>(change);
    unsigned int level_before = std::get<1>(change);
    unsigned int level_after = std::get<2>(change);

    if (level_before > level_after) {
      for (unsigned int level = level_before ; level > level_after ; --level) {
        BOOST_LOG(l.d(5)) << "--> Subtracting from " << level << ": " << i ;

        this->levels[level].subtract(i);
      }
    } else if (level_after > level_before) {
      for (unsigned int level = level_before + 1; level <= level_after ; ++level) {
        BOOST_LOG(l.d(5)) << "--> Adding to " << level << ": " << i ;
        this->levels[level].add(i);
      }
    }
  }
}

template<class Key, class LevelAssigner>
unsigned int
LevelTree<Key, LevelAssigner>::level_at(Key point)
{
  auto it = this->content.find(point);
  if (it == this->content.end()) {
    return 0;
  }
  return this->level_assigner.get_level(it->second.get_amount());
}

template<class Key, class LevelAssigner>
bool
LevelTree<Key, LevelAssigner>::strictly_before(const Interval &lhs, const Interval &rhs)
{
  // TODO works only with right-open intervals!
  return lhs.upper() <= rhs.lower();
}

template<class Key, class LevelAssigner>
bool
LevelTree<Key, LevelAssigner>::strictly_after(const Interval &lhs, const Interval &rhs)
{
  // TODO works only with right-open intervals!
  return lhs.lower() >= rhs.upper();
}

template<class Key, class LevelAssigner>
bool
LevelTree<Key, LevelAssigner>::contains(const Interval &lhs, const Interval &rhs)
{
  return ((lhs.lower() <= rhs.lower()) && (lhs.upper() >= rhs.upper()));
}

template<class Key, class LevelAssigner>
bool
LevelTree<Key, LevelAssigner>::operator==(const LevelTree<Key, LevelAssigner> & rhs)
{
  return (this->content == rhs.content) && (this->level_assigner == rhs.level_assigner) && (this->horizon == rhs.horizon);
}

template<class Key, class LevelAssigner>
void
LevelTree<Key, LevelAssigner>::dbg_verify() {
#ifndef ENABLE_CONSISTENCY_CHECKS
  return;
#else
  this->dbg_verify_level_assignment();
  this->dbg_verify_level_containment();
#endif
}

template<class Key, class LevelAssigner>
void
LevelTree<Key, LevelAssigner>::dbg_verify_level_assignment() {
#ifndef ENABLE_CONSISTENCY_CHECKS
  return;
#else
  const auto profile = this->get_profile();

  for (const auto & item : profile) {
    const auto & interval = std::get<0>(item);
    unsigned int level = std::get<1>(item);

    auto hit = this->levels[level].find(interval);
    assert(hit != this->levels[level].end());
    assert(this->contains(*hit, interval));

    if (level < this->level_assigner.level_count()) {
      auto miss = this->levels[level + 1].find(interval);
      assert(miss == this->levels[level + 1].end());
    }
  }
#endif
}

template<class Key, class LevelAssigner>
void
LevelTree<Key, LevelAssigner>::dbg_verify_level_containment() {
#ifndef ENABLE_CONSISTENCY_CHECKS
  return;
#else
  for (unsigned int level = 1 ; level <= this->level_assigner.level_count() ; ++level) {
    const auto & lower = this->get_level(level - 1);
    const auto & upper = this->get_level(level);

    // TODO write unit tests for binary predicates!
    const auto & diff = upper - lower;

    assert(diff.empty());
  }
#endif
}

template<class Key, class LevelAssigner, class _CharT, class _Traits>
std::basic_ostream<_CharT, _Traits>& operator<<(std::basic_ostream<_CharT, _Traits>& stream, const typename LevelTree<Key, LevelAssigner>::Interval & interval)
{
  stream << "[";
  stream << interval.lower();
  stream << ",";
  stream << interval.upper();
  stream << ")";

  return stream;
}

// explicit instantiation
template class LevelTree<unsigned int, level_tree::LinearLevelAssigner>;