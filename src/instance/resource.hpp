#ifndef RESOURCE_HPP
#define RESOURCE_HPP

#include "generated_config.hpp"

#include <boost/container/small_vector.hpp>
#include <type_traits>
#include <utility> // for pair
#include <vector>  // for allocator, vector

// one term: coefficient, exponent
typedef std::pair<double, double> poly_term;

// list of terms
// TODO make number of optimized poly-terms a compile-time variable?
typedef std::vector<poly_term> polynomial;

double apply_polynomial(const polynomial & poly, double x);
// TODO add moving variant for efficiency
polynomial add_poly(const polynomial & lhs, const polynomial & rhs);

// Forwards
class Instance;

class Availability {
public:
	Availability(double start_amount);
	Availability(const Availability & other) = default; // copy constructor

	void set(std::vector<std::pair<unsigned int, double>> && new_points);
	double get_at(unsigned int point) const noexcept;
	
	double get_flat_available() const;

	std::vector<std::pair<unsigned int, double>>::const_iterator begin() const;
	std::vector<std::pair<unsigned int, double>>::const_iterator end() const;

private:
	/* Each pair in the points vector is one step of a stepwise function
	 * indicating the availability of a resource. The first member of every point
	 * indicates from which time step on the amount is available, the second
	 * member indicates the amount. The first point (i.e., the first member of the
	 * first point) must be 0. The points must be sorted by ascending time steps.
	 */
	std::vector<std::pair<unsigned int, double>> points;
};

class FlexCost {
public:
	FlexCost(polynomial base);

	void
	set_flexible(std::vector<std::pair<unsigned int, polynomial>> && new_points);

	const polynomial & get_at(unsigned int point) const noexcept;
	const polynomial & get_base() const noexcept;
	
	bool is_flat() const;

	std::vector<std::pair<unsigned int, polynomial>>::const_iterator
	begin() const;
	std::vector<std::pair<unsigned int, polynomial>>::const_iterator end() const;

private:
	polynomial base;
	// Same idea as for Availability
	std::vector<std::pair<unsigned int, polynomial>> points;
};

class Resource {
public:
	explicit Resource(unsigned int id);

	void set_availability(Availability && availability);
	void set_overshoot_costs(FlexCost && cost);
	void set_investment_costs(polynomial costs);

	const Availability & get_availability() const;

	const FlexCost & get_flex_overshoot() const;
	const polynomial & get_overshoot_costs(unsigned int pos) const;

	// This only works if the instance has flat costs!
	const polynomial & get_overshoot_costs() const;
	bool is_overshoot_flat() const;

	const polynomial & get_investment_costs() const;

	unsigned int get_rid();
	void set_id(unsigned int id);

	bool
	operator==(const Resource & other) const
	{
		return other.rid == this->rid;
		// TODO in debug mode, compare everything!
	}

	// deepcopy
	Resource clone() const;

private:
	unsigned int rid;
	Availability availability;
	polynomial investment_costs;
	FlexCost overshoot_costs;
};

class ResVec
    : public boost::container::small_vector<double, OPTIMAL_RESOURCE_COUNT> {
public:
	using boost::container::small_vector<double,
	                                     OPTIMAL_RESOURCE_COUNT>::small_vector;
};

class Resources {
public:
	Resources();
	Resources(double usage);
	Resources(const Instance * instance, const ResVec & usage);
	Resources(const Instance * instance, const std::vector<double> & usage);
	Resources(const Instance * instance, ResVec && usage);
	Resources(const Instance * instance);

	const ResVec & getUsage() const;
	ResVec & getUsage();

	Resources operator+(const Resources & other) const;
	Resources operator-(const Resources & other) const;
	Resources operator*(const Resources & other) const;
	Resources operator/(const Resources & other) const;
	void operator+=(const Resources & other);
	void operator-=(const Resources & other);
	void operator*=(const Resources & other);
	void operator/=(const Resources & other);

	bool operator<(const Resources & other) const;
	bool operator>(const Resources & other) const;
	bool operator<=(const Resources & other) const;
	bool operator>=(const Resources & other) const;
	bool operator!=(const Resources & other) const;
	bool operator==(const Resources & other) const;

	template <typename T>
	inline friend std::enable_if_t<std::is_integral_v<T>, Resources>
	operator*(const T & scalar, const Resources & resource);

private:
	const Instance * instance;
	mutable bool cached;
	mutable double cache;

	ResVec usage;

	double getCosts() const;
};

template <typename T>
inline std::enable_if_t<std::is_integral_v<T>, Resources>
operator*(const T & scalar, const Resources & resource)
{
	Resources res(resource.instance, resource.usage);
	for (size_t i = 0; i < resource.usage.size(); i++) {
		res.usage[i] *= scalar;
	}
	return res;
}

template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
Resources
operator*(const Resources & resource, const T & scalar)
{
	return scalar * resource;
}

#endif
