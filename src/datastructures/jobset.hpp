#ifndef JOBSET_H
#define JOBSET_H

#include <iostream>          // for basic_ostream
#include <set>               // for set
#include <utility>           // for pair
#include "../instance/job.hpp"  // for Job, Job::JobId

class JobSet {
public:
  using JobId = Job::JobId;

  // Default constructor. *Must* spit out the neutral element!
  JobSet();
  JobSet(JobId job, double amount); // initialize with only one job

  inline void add(JobId job, double amount);
  inline void remove(JobId job, double amount);
  JobSet & operator+=(const JobSet & rhs);
  JobSet & operator-=(const JobSet & rhs);

  const auto & get() const;

  double get_amount() const;

  // TODO speed this up by hashing
  bool operator==(const JobSet &rhs) const;
private:
  std::set<std::pair<JobId, double>> content;
  double amount;

  template<class _CharT, class _Traits>
  friend std::basic_ostream<_CharT, _Traits>& operator<<(std::basic_ostream<_CharT, _Traits>& stream, const JobSet & jobset);
};

/*
 * Debug output methods
 */

template<class _CharT, class _Traits>
std::basic_ostream<_CharT, _Traits>&
operator<<(std::basic_ostream<_CharT, _Traits>& stream, const JobSet & jobset)
{
  stream << "JS([";
  bool first = true;
  for (const auto & entry : jobset.content) {
    if (!first) {
      stream << ", ";
    }

    stream << "(" << entry.first << ": " << entry.second << ")";
    first = false;
  }

  stream << "], " << jobset.amount << ")";
  return stream;
}
#endif
