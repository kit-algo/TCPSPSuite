#include "resource.hpp"

#include "instance.hpp"
#include "traits.hpp"
#include <cassert> // for assert
#include <cmath>   // for pow
#include <memory>
#include <stdexcept>

// TODO factor polynomial stuff out into util
double
apply_polynomial(const polynomial & poly, double x)
{
  double sum = 0;
  for (auto term : poly) {
    sum += std::get<0>(term) * std::pow(x, std::get<1>(term));
  }
  return sum;
}

Resource::Resource(unsigned int id) : rid(id), availability(0) {}

Resource
Resource::clone() const
{
  Resource cloned(this->rid);
  cloned.set_availability(Availability(this->availability));
  cloned.set_investment_costs(this->investment_costs);
  cloned.set_overshoot_costs(this->overshoot_costs);

  return cloned;
}

void
Resource::set_availability(Availability && availability_in)
{
  this->availability = std::move(availability_in);
}

const Availability &
Resource::get_availability() const
{
  return this->availability;
}

unsigned int
Resource::get_rid()
{
  return this->rid;
}

void
Resource::set_id(unsigned int id)
{
  this->rid = id;
}

void
Resource::set_investment_costs(polynomial costs)
{
  this->investment_costs = costs;
}

void
Resource::set_overshoot_costs(polynomial costs)
{
  this->overshoot_costs = costs;
}

const polynomial &
Resource::get_investment_costs() const
{
  return this->investment_costs;
}

const polynomial &
Resource::get_overshoot_costs() const
{
  return this->overshoot_costs;
}

Availability::Availability(double start_amount) : points(1, {0, start_amount})
{}

void
Availability::set(std::vector<std::pair<unsigned int, double>> && new_points)
{
  this->points = std::move(new_points);
}

std::vector<std::pair<unsigned int, double>>::const_iterator
Availability::begin() const
{
  return this->points.begin();
}

std::vector<std::pair<unsigned int, double>>::const_iterator
Availability::end() const
{
  return this->points.end();
}

double
Availability::get_flat_available() const
{
  assert(this->points.size() == 1);
  return this->points[0].second;
}

Resources::Resources() {}

Resources::Resources(double _usage) : usage{_usage} {}

Resources::Resources(const Instance * in, const ResVec & u)
    : instance(in), usage(u)
{
  if (in == NULL && u.size() > 1) {
    throw std::invalid_argument("Instance in is NULL");
  } else if (u.size() > 1 &&
             !in->get_traits().has_flag(Traits::FLAT_AVAILABILITY)) {
    throw TraitUnfulfilledError("FLAT_AVAILABILITY required!");
  }
}

Resources::Resources(const Instance * in, const std::vector<double> & u)
    : instance(in), usage(u.begin(), u.end())
{
  if (in == NULL && u.size() > 1) {
    throw std::invalid_argument("Instance in is NULL");
  } else if (u.size() > 1 &&
             !in->get_traits().has_flag(Traits::FLAT_AVAILABILITY)) {
    throw TraitUnfulfilledError("FLAT_AVAILABILITY required!");
  }
}

Resources::Resources(const Instance * in, ResVec && u)
    : instance(in), usage(std::move(u))
{
  if (in == NULL && u.size() > 1) {
    throw std::invalid_argument("Instance in is NULL");
  } else if (u.size() > 1 &&
             !in->get_traits().has_flag(Traits::FLAT_AVAILABILITY)) {
    throw TraitUnfulfilledError("FLAT_AVAILABILITY required!");
  }
}

Resources::Resources(const Instance * in)
    : instance(in), usage(in->resource_count(), 0.0)
{}

const ResVec &
Resources::getUsage() const
{
  return usage;
}

ResVec &
Resources::getUsage()
{
  return usage;
}

Resources
Resources::operator+(const Resources & other) const
{
  Resources res(*this);
  res += other;
  return res;
}

void
Resources::operator+=(const Resources & other)
{
  if (instance == NULL) {
    instance = other.instance;
  }
  if (usage.size() < other.usage.size()) {
    usage.resize(other.usage.size());
  }
  for (size_t i = 0; i < other.usage.size(); i++) {
    usage[i] += other.usage[i];
  }
}

void
Resources::operator-=(const Resources & other)
{
  if (instance == NULL) {
    instance = other.instance;
  }
  if (usage.size() < other.usage.size()) {
    usage.resize(other.usage.size());
  }
  for (size_t i = 0; i < other.usage.size(); i++) {
    usage[i] -= other.usage[i];
  }
}

bool
Resources::operator<(const Resources & other) const
{
  return this->getCosts() < other.getCosts();
}

bool
Resources::operator>(const Resources & other) const
{
  return this->getCosts() > other.getCosts();
}

bool
Resources::operator!=(const Resources & other) const
{
  return this->usage != other.usage;
}

bool
Resources::operator==(const Resources & other) const
{
  return this->usage == other.usage;
}

double
Resources::getCosts() const
{
  if (usage.size() == 0) {
    return 0;
  } else if (usage.size() == 1) {
    return usage[0];
  } else if (!cached) {
    cache = instance->calculate_costs(usage);
    cached = true;
  }
  return cache;
}
