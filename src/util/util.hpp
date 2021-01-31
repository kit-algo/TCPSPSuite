#ifndef UTIL_H
#define UTIL_H

#include "generated_config.hpp"
#include <cstddef>
#include <array>
#include <functional>

// consts-cast for range-based for on const objects
template<typename T> constexpr const T &as_const(T &t) noexcept { return t; }

// static array size determination
// straight from emc++ ;)
template<typename T, std::size_t N>
constexpr std::size_t get_array_size(T (&)[N]) noexcept
{
  return N;
}

// Hashing std::array
// taken from https://stackoverflow.com/questions/8027368/are-there-no-specializations-of-stdhash-for-standard-containers
namespace std
{
  template<typename T, size_t N>
  struct hash<array<T, N> >
  {
    typedef array<T, N> argument_type;
    typedef size_t result_type;

    result_type operator()(const argument_type& a) const
    {
      hash<T> hasher;
      result_type h = 0;
      for (result_type i = 0; i < N; ++i)
      {
        h = h * 31 + hasher(a[i]);
      }
      return h;
    }
  };
}

// clang somehow treats log2 as non-constexpr, which sucks.
constexpr size_t clog2(size_t n)
{
  return ( (n<2) ? 0 : 1+clog2(n/2));
}

inline constexpr bool double_eq(double lhs, double rhs) {
	return (lhs <= rhs + DOUBLE_DELTA) && (lhs >= rhs - DOUBLE_DELTA);
}

#endif
