//
// Created by lukas on 14.12.17.
//

#include "log.hpp"
#include <shared_mutex>
#include <boost/log/attributes/value_extraction.hpp>
#include <boost/log/attributes/value_extraction_fwd.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_feature.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>

std::shared_timed_mutex Log::thread_id_map_mutex;
std::map<boost::log::attributes::current_thread_id::value_type, unsigned int>
    Log::thread_id_map;

std::locale
Log::get_locale() noexcept
{
  static std::locale time_format_loc(
      std::wcout.getloc(), new boost::posix_time::wtime_facet(L"%H:%M:%S"));
  return time_format_loc;
}

void
Log::coloring_formatter(boost::log::record_view const & rec,
                        boost::log::formatting_ostream & strm) noexcept
{
  auto sev_lvl = boost::log::extract<Log::severity>("Severity", rec);
  if (sev_lvl) {
    // Set the color
    switch (sev_lvl.get()) {
    case Log::severity::debug:
      strm << COLOR_DEBUG;
      break;
    case Log::severity::normal:
    case Log::severity::info:
      strm << COLOR_INFO;
      break;
    case Log::severity::warning:
      strm << COLOR_WARNING;
      break;
    case Log::severity::error:
    case Log::severity::fatal:
      strm << COLOR_ERROR;
      break;
    default:
      break;
    }
  }

  boost::log::value_ref<boost::log::attributes::current_thread_id::value_type>
      thread_id_ref = boost::log::extract<
          boost::log::attributes::current_thread_id::value_type>("ThreadID",
                                                                 rec);
  int thread_id = -1;
  if (thread_id_ref) {
    auto internal_thread_id = thread_id_ref.get();

    std::shared_lock<std::shared_timed_mutex> read_lock(
        Log::thread_id_map_mutex);

    auto thread_id_it = Log::thread_id_map.find(internal_thread_id);
    if (thread_id_it == Log::thread_id_map.end()) {
      read_lock.unlock();
      std::unique_lock<std::shared_timed_mutex> write_lock(
          Log::thread_id_map_mutex);

      // Make sure this is still true.
      thread_id_it = Log::thread_id_map.find(internal_thread_id);
      if (thread_id_it != Log::thread_id_map.end()) {
	// someone added it in the meantime!
	thread_id = (int)thread_id_it->second;
      } else {
	thread_id = (int)Log::thread_id_map.size();
	Log::thread_id_map.insert({internal_thread_id, thread_id});
      }

      write_lock.unlock();
    } else {
      thread_id = (int)thread_id_it->second;
      read_lock.unlock();
    }
  }

#if Boost_MAJOR_VERSION > 1 ||                                                 \
    (Boost_MAJOR_VERSION > 0 && Boost_MINOR_VERSION >= 64)
  std::array<std::pair<const char *, const char *>, 1> newline_escapes = {
      {{"\n", "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~   "}},
  };

  boost::log::formatter f =
      boost::log::expressions::stream
      << std::setfill(' ') << "["
      << boost::log::expressions::format_date_time<boost::posix_time::ptime>(
             "TimeStamp", "%H:%M:%S")
      << "]"
      << "[" << std::setw(3) << thread_id << "]"
      << "[" << std::setw(6)
      << boost::log::expressions::max_size_decor<char>(
             6)[boost::log::expressions::stream
                << boost::log::expressions::attr<std::string>("component")]
      << "]"
      << "   "
      << boost::log::expressions::char_decor(
             newline_escapes)[boost::log::expressions::stream
                              << boost::log::expressions::smessage];
  f(rec, strm);
#else
  strm << std::setfill(' ') << "["
       << boost::log::extract<boost::posix_time::ptime>("TimeStamp", rec) << "]"
       << "[" << std::setw(3) << thread_id << "]"
       << "[" << boost::log::extract<std::string>("component", rec) << "]"
       << rec[boost::log::expressions::smessage];
#endif

  if (sev_lvl) {
    // Restore the default color
    strm << "\033[0m";
  }
}

void
Log::setup() noexcept
{
  boost::log::add_common_attributes();
  auto console_sink = boost::log::add_console_log();

  boost::log::core::get()->set_filter(
      !boost::log::expressions::has_attr<bool>("filter_out"));
  console_sink->set_formatter(&Log::coloring_formatter);
}

Log::Log(std::string component) noexcept
    : i_logger(boost::log::keywords::severity = severity::info),
      n_logger(boost::log::keywords::severity = severity::normal),
      w_logger(boost::log::keywords::severity = severity::warning),
      e_logger(boost::log::keywords::severity = severity::error),
      f_logger(boost::log::keywords::severity = severity::fatal)
{
  for (unsigned int i = 0; i < MAX_DBG_LEVEL; ++i) {
    Log::d_logger[i] = boost::log::sources::severity_logger<severity>(
        boost::log::keywords::severity = severity::debug);
    Log::d_logger[i].add_attribute(
        "debug_level", boost::log::attributes::constant<unsigned int>(i));
    Log::d_logger[i].add_attribute(
        "component", boost::log::attributes::constant<std::string>(component));
  }

  Log::i_logger.add_attribute(
      "component", boost::log::attributes::constant<std::string>(component));
  Log::n_logger.add_attribute(
      "component", boost::log::attributes::constant<std::string>(component));
  Log::w_logger.add_attribute(
      "component", boost::log::attributes::constant<std::string>(component));
  Log::e_logger.add_attribute(
      "component", boost::log::attributes::constant<std::string>(component));
  Log::f_logger.add_attribute(
      "component", boost::log::attributes::constant<std::string>(component));

  filtered_logger.add_attribute("filter_out",
                                boost::log::attributes::constant<bool>(true));
}

boost::log::sources::severity_logger<Log::severity> &
Log::d(unsigned int level) const noexcept
{
  if (level < MAX_DBG_LEVEL) {
    return const_cast<boost::log::sources::severity_logger<severity> &>(
        Log::d_logger[level]);
  } else {
    return const_cast<boost::log::sources::severity_logger<severity> &>(
        Log::filtered_logger);
  }
}

boost::log::sources::severity_logger<Log::severity> &
Log::i() const noexcept
{
  return const_cast<boost::log::sources::severity_logger<severity> &>(
      Log::i_logger);
}

boost::log::sources::severity_logger<Log::severity> &
Log::n() const noexcept
{
  return const_cast<boost::log::sources::severity_logger<severity> &>(
      Log::n_logger);
}

boost::log::sources::severity_logger<Log::severity> &
Log::w() const noexcept
{
  return const_cast<boost::log::sources::severity_logger<severity> &>(
      Log::w_logger);
}

boost::log::sources::severity_logger<Log::severity> &
Log::e() const noexcept
{
  return const_cast<boost::log::sources::severity_logger<severity> &>(
      Log::e_logger);
}

boost::log::sources::severity_logger<Log::severity> &
Log::f() const noexcept
{
  return const_cast<boost::log::sources::severity_logger<severity> &>(
      Log::f_logger);
}
