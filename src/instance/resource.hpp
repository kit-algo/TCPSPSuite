#ifndef RESOURCE_HPP
#define RESOURCE_HPP

#include <utility>  // for pair
#include <vector>   // for allocator, vector

// one term: coefficient, exponent
typedef std::pair<double, double> poly_term;

// list of terms
typedef std::vector<poly_term> polynomial;

double apply_polynomial(const polynomial & poly, double x);

namespace instance {
class Availability {
public:
  Availability(double start_amount);
  Availability(const Availability & other) = default; // copy constructor

  void set(std::vector<std::pair<unsigned int, double>> && new_points);

  double get_flat_available() const;

  std::vector<std::pair<unsigned int, double>>::const_iterator begin() const;
  std::vector<std::pair<unsigned int, double>>::const_iterator end() const;
private:
  /* Each pair in the points vector is one step of a stepwise function indicating the availability
   * of a resource. The first member of every point indicates from which time step on the amount
   * is available, the second member indicates the amount. The first point (i.e., the first
   * member of the first point) must be 0. The points must be sorted by ascending time steps.
   */
  std::vector<std::pair<unsigned int, double>> points;
};
}

// TODO move into namespace
class Resource {
public:
  explicit Resource(unsigned int id);

  void set_availability(instance::Availability && availability);

  void set_overshoot_costs(polynomial costs);
  void set_investment_costs(polynomial costs);

  const instance::Availability & get_availability() const;

  const polynomial & get_overshoot_costs() const;
  const polynomial & get_investment_costs() const;

  unsigned int get_rid();
  void set_id(unsigned int id);

  bool operator==(const Resource &other) const {
    return other.rid == this->rid;
    // TODO in debug mode, compare everything!
  }

  // deepcopy
  Resource clone() const;

private:
  unsigned int rid;
  instance::Availability availability;
  polynomial investment_costs;
  polynomial overshoot_costs;
};

#endif
