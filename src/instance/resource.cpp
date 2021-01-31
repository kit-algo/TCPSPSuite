#include "resource.hpp"

#include "instance.hpp"
#include "traits.hpp"

#include <algorithm>
#include <boost/container/small_vector.hpp>
#include <cassert> // for assert
#include <cmath>   // for pow
#include <iterator>
#include <memory>
#include <pstl/glue_algorithm_defs.h>
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

polynomial
add_poly(const polynomial & lhs, const polynomial & rhs)
{
	polynomial result(lhs.begin(), lhs.end());
	result.insert(result.end(), rhs.begin(), rhs.end());
	std::sort(result.begin(), result.end(),
	          [](const poly_term & sort_lhs, const poly_term & sort_rhs) {
		          return sort_lhs.second < sort_rhs.second;
	          });

	size_t len = result.size();
	for (size_t i = 0; (len > 0) && (i < (len - 1)); ++i) {
		// TODO float-equal comparison.
		if (double_eq(result[i].second, result[i + 1].second)) {
			// Equal exponents. Combine, mark for swap-and-delete
			result[i].first += result[i + 1].first;
			result[i + 1].first = 0;
			i++;
		}
	}

	auto new_end =
	    std::remove_if(result.begin(), result.end(), [](const poly_term & t) {
		    return double_eq(t.first, 0.0);
	    });
	result.erase(new_end, result.end());

	return result;
}
// TODO test add_poly

Resource::Resource(unsigned int id)
    : rid(id), availability(0), overshoot_costs({})
{}

Resource
Resource::clone() const
{
	Resource cloned(this->rid);
	cloned.set_availability(Availability(this->availability));
	cloned.set_investment_costs(this->investment_costs);
	cloned.set_overshoot_costs(FlexCost(this->overshoot_costs));

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
Resource::set_overshoot_costs(FlexCost && costs)
{
	this->overshoot_costs = std::move(costs);
}

const polynomial &
Resource::get_investment_costs() const
{
	return this->investment_costs;
}

const polynomial &
Resource::get_overshoot_costs() const
{
	return this->overshoot_costs.get_base();
}

const FlexCost &
Resource::get_flex_overshoot() const
{
	return this->overshoot_costs;
}

const polynomial &
Resource::get_overshoot_costs(unsigned int pos) const
{
	return this->overshoot_costs.get_at(pos);
}

bool
Resource::is_overshoot_flat() const
{
	return this->overshoot_costs.is_flat();
}

FlexCost::FlexCost(polynomial base_in) : base(std::move(base_in)) {}

bool
FlexCost::is_flat() const
{
	return this->points.empty();
}

const polynomial &
FlexCost::get_base() const noexcept
{
	return this->base;
}

void
FlexCost::set_flexible(
    std::vector<std::pair<unsigned int, polynomial>> && new_points)
{
	if (this->base.empty()) {
		this->points = std::move(new_points);
	} else {
		this->points.reserve(new_points.size());
		this->points.clear();

		std::transform(new_points.begin(), new_points.end(),
		               std::back_inserter(this->points),
		               [&](const std::pair<unsigned int, polynomial> point) {
			               return std::pair<unsigned int, polynomial>(
			                   point.first, add_poly(this->base, point.second));
		               });
	}
}

const polynomial &
FlexCost::get_at(unsigned int pos) const noexcept
{
	if (this->points.empty()) {
		return this->base;
	} else {

		/* This comparator inverses comparison s.t. we can just use
		 * lower_bound on the reversed points to find the correct point. */
		struct Comp
		{
			bool
			operator()(const std::pair<unsigned int, polynomial> & lhs,
			           unsigned int rhs) const noexcept
			{
				return lhs.first > rhs;
			}
			bool
			operator()(unsigned int lhs,
			           const std::pair<unsigned int, polynomial> & rhs) const noexcept
			{
				return lhs > rhs.first;
			}
		};

		auto it = std::lower_bound(this->points.rbegin(), this->points.rend(), pos,
		                           Comp{});
		if (it == this->points.rend()) {
			return this->base;
		} else {
			return it->second;
		}
	}
}
// TODO test get_at

std::vector<std::pair<unsigned int, polynomial>>::const_iterator
FlexCost::begin() const
{
	return this->points.begin();
}
std::vector<std::pair<unsigned int, polynomial>>::const_iterator
FlexCost::end() const
{
	return this->points.end();
}

Availability::Availability(double start_amount) : points(1, {0, start_amount})
{}

double
Availability::get_at(unsigned int pos) const noexcept
{
	if (this->points.empty()) {
		return 0;
	} else {

		/* This comparator inverses comparison s.t. we can just use
		 * lower_bound on the reversed points to find the correct point. */
		struct Comp
		{
			bool
			operator()(const std::pair<unsigned int, double> & lhs,
			           unsigned int rhs) const noexcept
			{
				return lhs.first > rhs;
			}
			bool
			operator()(unsigned int lhs,
			           const std::pair<unsigned int, double> & rhs) const noexcept
			{
				return lhs > rhs.first;
			}
		};

		auto it = std::lower_bound(this->points.rbegin(), this->points.rend(), pos,
		                           Comp{});
		if (it == this->points.rend()) {
			return 0.0;
		} else {
			return it->second;
		}
	}
}

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

Resources
Resources::operator-(const Resources & other) const
{
	Resources res(*this);
	res -= other;
	return res;
}

Resources
Resources::operator*(const Resources & other) const
{
	Resources res(*this);
	res *= other;
	return res;
}

Resources
Resources::operator/(const Resources & other) const
{
	Resources res(*this);
	res /= other;
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
Resources::operator*=(const Resources & other)
{
	if (instance == NULL) {
		instance = other.instance;
	}
	if (usage.size() < other.usage.size()) {
		usage.resize(other.usage.size());
	}
	for (size_t i = 0; i < other.usage.size(); i++) {
		usage[i] *= other.usage[i];
	}
}

void
Resources::operator/=(const Resources & other)
{
	if (instance == NULL) {
		instance = other.instance;
	}
	if (usage.size() < other.usage.size()) {
		usage.resize(other.usage.size());
	}
	for (size_t i = 0; i < other.usage.size(); i++) {
		usage[i] /= other.usage[i];
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
Resources::operator<=(const Resources & other) const
{
	return this->getCosts() <= other.getCosts();
}

bool
Resources::operator>=(const Resources & other) const
{
	return this->getCosts() >= other.getCosts();
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
