#ifndef TRAITS_H
#define TRAITS_H

#include <array>                        // for array
#include <deque>                        // for deque
#include <ostream>                      // for basic_ostream
#include <set>                          // for set
#include <stdexcept>                    // for runtime_error
#include <string>                       // for string
#include <unordered_map>                // for unordered_map
#include <utility>                      // for pair
#include <vector>                       // for vector, allocator
#include "../datastructures/maybe.hpp"  // for Maybe
#include "../util/util.hpp"             // for get_array_size, clog2
class Instance;  // lines 15-15
class Transformer;  // lines 16-16

class TraitViolatedError : public std::runtime_error
{
public:
  explicit TraitViolatedError(const std::string& what);
  explicit TraitViolatedError(const char *what);
};

class TraitUnfulfilledError : public std::runtime_error
{
public:
  explicit TraitUnfulfilledError(const std::string& what);
  explicit TraitUnfulfilledError(const char *what);
};

// TODO negative implications! E.g.: "NOT NO_DRAIN" => "NOT COMMON_DURATION"

class Traits {
public:
  // TODO use std::bitset

  /*
   * WARNING! If you modify something here, remember to:
   *    - update ALL_TRAIT_FLAGS
   *    - update the constants in traits.cpp
   *    - update *all* the traits_profile s in the transformers
   *    - update the checks in the TraitsBuilder
   */

  static const unsigned long NO_LAGS                    ;
  static const unsigned long LAGS_ONLY_SUCCESSORS       ;
  static const unsigned long LAGS_ONLY_GREATER_DURATION ;
  static const unsigned long LAGS_ONLY_POSITIVE         ;
  static const unsigned long LAGS_DAG                   ;

  static const unsigned long COMMON_RELEASE             ;
  static const unsigned long COMMON_DEADLINE            ;
  static const unsigned long COMMON_DURATION            ;

  static const unsigned long CONSISTENT_WINDOWS         ;

  static const unsigned long DUMMY_START_END            ;

  static const unsigned long NO_DRAIN                   ;

  static const unsigned long NO_WINDOW_EXTENSION        ;
  static const unsigned long WINDOW_EXTENSION_JOBS_UNLIMITED ;

  static const unsigned long FLAT_AVAILABILITY          ;
  static const unsigned long ZERO_AVAILABILITY          ;

  // End of flags

  static const char* const FLAG_NAMES[]                 ;

  Traits(unsigned long flags, unsigned int max_resources, std::set<double> allowed_overshoot_exponents,
    std::set<double> allowed_investment_exponents);
  Traits();

  bool fulfills(const Traits &requirements) const;
  bool has_flag(unsigned long flag) const;

  void add_flag(unsigned long flag);
  void remove_flag(unsigned long flag);

  // deepcopy
  Traits clone() const;

  // String output
  template<class _CharT, class _Traits>
  friend std::basic_ostream<_CharT, _Traits>& operator<<(std::basic_ostream<_CharT, _Traits>& stream, const Traits & traits);

private:
  unsigned long flags;
  unsigned int max_resources;

  std::set<double> allowed_overshoot_exponents;
  std::set<double> allowed_investment_exponents;

  static const std::vector<std::pair<unsigned long, unsigned long>> implications;
};

// Helper to define profiles
const unsigned long ALL_TRAIT_FLAGS[] = {
  Traits::NO_LAGS, Traits::LAGS_ONLY_SUCCESSORS,
  Traits::LAGS_ONLY_GREATER_DURATION, Traits::LAGS_ONLY_POSITIVE, Traits::LAGS_DAG,
  Traits::COMMON_RELEASE, Traits::COMMON_DEADLINE, Traits::COMMON_DURATION, Traits::DUMMY_START_END,
  Traits::NO_DRAIN, Traits::CONSISTENT_WINDOWS, Traits::NO_WINDOW_EXTENSION,
  Traits::WINDOW_EXTENSION_JOBS_UNLIMITED, Traits::FLAT_AVAILABILITY, Traits::ZERO_AVAILABILITY
};

class TraitsBuilder {
public:
  explicit TraitsBuilder(const Instance &instance);
  void run();
  Traits get_traits();

private:
  void check_no_lags();
  void check_lag_durations();
  void check_lag_dag();
  void check_no_drain();
  void check_deadline_release();
  void check_consistent_windows();
  void check_dummy_start_end();
  void check_window_extension();
  void check_availabilities();

  unsigned long flags;
  const Instance &instance;
};

class TraitsRouter {
public:
  static const unsigned short WANT_MAYBE                ;
  static const unsigned short WANT_YES                  ;
  static const unsigned short WANT_NO                   ;

  using trait_profile = std::array<bool, get_array_size(ALL_TRAIT_FLAGS)>;
  using transform_profile = std::array<unsigned short, get_array_size(ALL_TRAIT_FLAGS)>;

  explicit TraitsRouter(const std::set<Transformer *> & transformers);
  Maybe<std::vector<Transformer *>> get_path(const Traits & from, const Traits & to);

private:
  const std::set<Transformer *> & transformers;

  std::vector<std::pair<transform_profile, Transformer *>> in_edges;
  std::unordered_map<trait_profile, std::pair<Transformer *, trait_profile>> tree;
  std::deque<trait_profile> queue;
  trait_profile final_profile;
  bool found;

  void build_in_edges();
  void do_bfs(const Traits & from, const Traits & to);
  bool match(const trait_profile & traits_p, const transform_profile & trans_p) const;
  bool fulfills(const trait_profile & profile, const trait_profile & pattern) const;
  std::vector<Transformer *> find_matching(const trait_profile & profile) const;
  trait_profile transform_flags(const trait_profile & in_profile, const transform_profile & transform) const;
  trait_profile traits_to_profile(const Traits & traits) const;
};


/*
 * Debug output methods
 */

// Maps e.g. Traits::LAGS_ONLY_GREATER_DURATION to its index, i.e. 2
// this should be evaluated at compile time by any sane compiler
constexpr unsigned int flag_to_index(unsigned long flag) {
 return (unsigned int)clog2(flag);
}

template<class _CharT, class _Traits>
std::basic_ostream<_CharT, _Traits>&
operator<<(std::basic_ostream<_CharT, _Traits>& stream, const Traits & traits)
{
  stream << "Traits[ ";

  bool first = true;
  for (auto flag : ALL_TRAIT_FLAGS) {
    if (traits.has_flag(flag)) {
      if (!first) {
        stream << " | ";
      }
      stream << Traits::FLAG_NAMES[flag_to_index(flag)];
      first = false;
    }
  }

  stream << " / " << traits.max_resources << " / {";

  first = true;
  for (auto exponent : traits.allowed_overshoot_exponents) {
    if (!first) {
      stream << ",";
    }
    stream << exponent;
    first = false;
  }

  stream << "} / {";

  first = true;
  for (auto exponent : traits.allowed_investment_exponents) {
    if (!first) {
      stream << ",";
    }
    stream << exponent;
    first = false;
  }

  stream << "} ]";

  return stream;
}

#endif
