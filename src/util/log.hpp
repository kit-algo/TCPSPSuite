#ifndef LOG_H
#define LOG_H

#define BOOST_LOG_DYN_LINK 1

#include "generated_config.hpp"

#include <cassert>
#include <ctime>
#include <fstream>
#include <functional>
#include <iostream>
#include <locale>
#include <mutex>
#include <ostream>
#include <set>
#include <shared_mutex>
#include <sstream>
#include <stdio.h>
#include <string.h>
#include <thread>
#include <utility>
#include <vector>

#include <boost/log/attributes/current_thread_id.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/date_time.hpp>

// red
#define COLOR_ERROR "\033[31m"
// yellow
#define COLOR_WARNING "\033[33m"
// green
#define COLOR_INFO "\033[32m"
// cyan
#define COLOR_DEBUG "\033[36m"

// Outputting tuples
template <class _CharT, class _Traits, typename Type, size_t N, size_t Last>
struct tuple_printer
{
  static void
  print(std::basic_ostream<_CharT, _Traits> & out, const Type & value) noexcept
  {
    out << std::get<N>(value) << ", ";
    tuple_printer<_CharT, _Traits, Type, N + 1, Last>::print(out, value);
  }
};

template <class _CharT, class _Traits, typename Type, size_t N>
struct tuple_printer<_CharT, _Traits, Type, N, N>
{
  static void
  print(std::basic_ostream<_CharT, _Traits> & out, const Type & value) noexcept
  {
    out << std::get<N>(value);
  }
};

template <class _CharT, class _Traits, typename... Types>
std::basic_ostream<_CharT, _Traits> &
operator<<(std::basic_ostream<_CharT, _Traits> & out,
           const std::tuple<Types...> & value) noexcept
{
  out << "{ ";
  tuple_printer<_CharT, _Traits, std::tuple<Types...>, 0,
                sizeof...(Types) - 1>::print(out, value);
  out << " }";
  return out;
}

template <class T, class _CharT, class _Traits>
std::basic_ostream<_CharT, _Traits> &
operator<<(std::basic_ostream<_CharT, _Traits> & os,
           const std::set<T> & set) noexcept
{
  os << "{ ";
  bool first = true;
  for (auto el : set) {
    if (!first) {
      os << ", ";
    }

    os << el;

    first = false;
  }
  os << " }";

  return os;
}

template <class T1, class T2, class _CharT, class _Traits>
std::basic_ostream<_CharT, _Traits> &
operator<<(std::basic_ostream<_CharT, _Traits> & os,
           const std::pair<T1, T2> & pair) noexcept
{
  os << "{ ";
  os << pair.first << ", " << pair.second;
  os << " }";
  return os;
}

template <class T, class _CharT, class _Traits>
std::basic_ostream<_CharT, _Traits> &
operator<<(std::basic_ostream<_CharT, _Traits> & os, const std::vector<T> & v) noexcept
{
  os << "< ";
  bool first = true;
  for (const auto & el : v) {
    if (!first) {
      os << ", ";
    }

    os << el;

    first = false;
  }
  os << " >";

  return os;
}

class Log {
public:
  enum class severity
  {
    debug,
    info,
    normal,
    warning,
    error,
    fatal
  };

private:
  // Map thread IDs to small numbers
  static std::map<boost::log::attributes::current_thread_id::value_type,
                  unsigned int>
      thread_id_map;
  static std::shared_timed_mutex thread_id_map_mutex;

  static std::locale get_locale() noexcept;

  static void
  coloring_formatter(boost::log::record_view const & rec,
                     boost::log::formatting_ostream & strm) noexcept;

public:
  static void setup() noexcept;

  Log(std::string component) noexcept;

  boost::log::sources::severity_logger<severity> &
  d(unsigned int level = 0) const noexcept;

  boost::log::sources::severity_logger<severity> & i() const noexcept;

  boost::log::sources::severity_logger<severity> & n() const noexcept;

  boost::log::sources::severity_logger<severity> & w() const noexcept;

  boost::log::sources::severity_logger<severity> & e() const noexcept;

  boost::log::sources::severity_logger<severity> & f() const noexcept;

private:
  // TODO check if this impacts performance!
  boost::log::sources::severity_logger<severity> filtered_logger;

  std::array<boost::log::sources::severity_logger<severity>, MAX_DBG_LEVEL>
      d_logger;
  boost::log::sources::severity_logger<severity> i_logger;
  boost::log::sources::severity_logger<severity> n_logger;
  boost::log::sources::severity_logger<severity> w_logger;
  boost::log::sources::severity_logger<severity> e_logger;
  boost::log::sources::severity_logger<severity> f_logger;
};

#endif
