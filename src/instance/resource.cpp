#include "resource.hpp"
#include <cassert>  // for assert
#include <cmath>    // for pow

double apply_polynomial(const polynomial & poly, double x) {
  double sum = 0;
  for(auto term : poly) {
    sum += std::get<0>(term) * std::pow(x, std::get<1>(term));
  }
  return sum;
}

Resource::Resource(unsigned int id)
 : rid(id), availability(0)
{
}

Resource
Resource::clone() const
{
  Resource cloned(this->rid);
  cloned.set_availability(instance::Availability(this->availability));
  cloned.set_investment_costs(this->investment_costs);
  cloned.set_overshoot_costs(this->overshoot_costs);

  return cloned;
}

void
Resource::set_availability(instance::Availability &&availability_in)
{
  this->availability = std::move(availability_in);
}

const instance::Availability&
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

namespace instance {
Availability::Availability(double start_amount)
  : points(1, {0, start_amount})
{}

void
Availability::set(std::vector<std::pair<unsigned int, double>> &&new_points)
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

}