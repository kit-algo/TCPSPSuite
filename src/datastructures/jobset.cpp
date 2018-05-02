#include "jobset.hpp"

JobSet::JobSet()
{}

JobSet::JobSet(JobId job, double amount_in)
  : content({{job, amount_in}}), amount(amount_in)
{}

void
JobSet::add(JobId job, double amount_in)
{
  this->amount += amount_in;
  this->content.insert(std::make_pair(job, amount_in));
}

void
JobSet::remove(JobId job, double amount_in)
{
  this->amount -= amount_in;
  this->content.erase(std::make_pair(job, amount_in));
}

JobSet &
JobSet::operator+=(const JobSet & rhs)
{
  for (const auto & entry : rhs.content) {
    auto it = this->content.find(entry);

    if (it == this->content.end()) {
      this->amount += entry.second;
      this->content.insert(entry);
    }
  }

  return (*this);
}

JobSet &
JobSet::operator-=(const JobSet & rhs)
{
  //L_DBG<5>("JOBSET") << "|| JobSet operator-=: " << *this << " - " << rhs << "\n";
  for (const auto & entry : rhs.content) {
    // TODO speed this up by iteration?
    auto it = this->content.find(entry);
    if (it != this->content.end()) {
      this->amount -= it->second;
      this->content.erase(it);
    }
  }

#ifdef ENABLE_ASSERTIONS
  assert(this->amount > -1 * DOUBLE_DELTA);
#endif

  //L_DBG<5>("JOBSET") << "|| Result: " << *this << "\n";

  return (*this);
}

const auto &
JobSet::get() const
{
  return this->content;
}

double
JobSet::get_amount() const
{
  return this->amount;
}

bool
JobSet::operator==(const JobSet &rhs) const
{
  return this->content == rhs.content;
}
