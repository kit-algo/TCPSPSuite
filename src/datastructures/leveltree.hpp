#ifndef LEVELTREE_H
#define LEVELTREE_H

#include <assert.h>                            // for assert
#include <boost/container/detail/std_fwd.hpp>  // for pair
#include <boost/icl/discrete_interval.hpp>     // for discrete_interval
#include <boost/icl/interval_map.hpp>          // for interval_map
#include <boost/icl/interval_set.hpp>          // for interval_set
#include <ostream>                             // for basic_ostream
#include <tuple>                               // for tuple
#include <vector>                              // for vector
#include "../instance/job.hpp"  // for Job, Job::JobId
#include "generated_config.hpp"                // for ENABLE_ASSERTIONS
#include "../util/log.hpp"                        // for Log
#include "jobset.hpp"

namespace level_tree {

class LinearLevelAssigner {
public:
  LinearLevelAssigner(double min, double max, unsigned int levels);
  unsigned int get_level(double amount) const;

  bool operator==(const LinearLevelAssigner & rhs) const;

  unsigned int level_count() const;

private:
  double min_val;
  double level_size;
  unsigned int levels;
};

} // namespace level_tree

template<class Key, class LevelAssigner>
class LevelTree {
public:
  using JobId = Job::JobId;
  using Interval = typename boost::icl::discrete_interval<Key>::type;
  using LevelProfile = std::vector<std::tuple<const Interval, unsigned int, double>>;
  using ContentData = std::vector<std::pair<const Interval, const JobSet>>;

  LevelTree(LevelAssigner level_assigner, Interval horizon);

  class Intervals {
  public:
    // TODO move them into a single template, only one operator is different!
    Intervals operator-(const Intervals &rhs) const;
    Intervals operator+(const Intervals &rhs) const;
    Intervals operator&(const Intervals &rhs) const;
    Intervals operator!() const;

    Intervals extend(Key extend_left, Key extend_right) const;
    Intervals move_left(Key amount) const;

    void push_back(Interval interval);
    const std::vector<Interval> & get() const;

    Key coverage() const;
    bool empty() const;

	  bool overlaps(const Interval & interval) const;

    // String output
    // Needs to be *defined* here :(
    template<class _CharT, class _Traits>
    friend std::basic_ostream<_CharT, _Traits>& operator<<(std::basic_ostream<_CharT, _Traits>& stream, const typename LevelTree<Key, LevelAssigner>::Intervals & intervals) {
      stream << "[ ";

      bool first = true;
      for (const auto & interval : intervals.get()) {
        if (!first) {
          stream << ", ";
        }
        stream << interval;
        first = false;
      }

      stream << " ]";

      return stream;
    }


  private:
    std::vector<Interval> content; // assumed to be ordered!

    template<class binop>
    Intervals operator_base(binop op, const Intervals & rhs) const;
  };

  static Interval make_interval(Key start, Key end)
  {
#ifdef ENABLE_ASSERTIONS
    assert(end > start);
#endif
    return Interval(start, end);
  }

  // TODO make this a move insert
  void insert(const Interval &interval, JobSet payload);
  void remove(const Interval &interval, const JobSet &payload);

  void insert(const Interval &interval, JobId job, double amount);
  void remove(const Interval &interval, JobId job, double amount);

  //void move(const Interval &old_interval, const Interval &new_interval, JobId job, double amount);

  unsigned int level_at(Key point);

  // TODO this requires copying…
  Intervals get_level(unsigned int level);
  LevelProfile get_profile(const Interval &query);
  LevelProfile get_profile();
  ContentData get_content();

  // operators
  bool operator==(const LevelTree<Key, LevelAssigner> & rhs);

  void dbg_verify();


  // String output
  // Needs to be *defined* here :(
  template<class _CharT, class _Traits>
  friend std::basic_ostream<_CharT, _Traits>& operator<<(std::basic_ostream<_CharT, _Traits>& stream, const typename LevelTree<Key, LevelAssigner>::Interval & interval);

private:
  void update_levels(const LevelProfile &oldp, const LevelProfile &newp);

  std::vector<boost::icl::interval_set<Key>> levels;
  boost::icl::interval_map<Key, JobSet> content;

  LevelAssigner level_assigner;
  // TODO should be const…
  Interval horizon;

  // Interval comparison
  bool strictly_before(const Interval &lhs, const Interval &rhs);
  bool strictly_after(const Interval &lhs, const Interval &rhs);
  bool contains(const Interval &lhs, const Interval &rhs);

  // TODO FIXME move to unit-test?
  void dbg_verify_level_containment();
  void dbg_verify_level_assignment();

  Log l;
};

#endif
