#include "db_factory.hpp"

#include "generated_config.hpp"

// MySQL is broken and uses some weird 'my_bool'
//using my_bool = bool;

#include <odb/exceptions.hxx>

#ifdef ODB_MYSQL_FOUND
#include <odb/mysql/database.hxx>
#endif

#ifdef ODB_SQLITE_FOUND
#include <odb/sqlite/database.hxx>
#endif

#include <odb/schema-catalog.hxx>
#include <odb/session.hxx>
#include <odb/transaction.hxx>

#include <regex>

DBFactory::DBFactory()
  : l("DBFACTORY")
{}

std::unique_ptr<odb::database>
DBFactory::try_get_sqlite(std::string uri, bool rw, bool create)
{
#ifdef ODB_SQLITE_FOUND
  int options = 0;
  if (rw) {
    options |= SQLITE_OPEN_READWRITE;
  }
  if (create) {
    options |= SQLITE_OPEN_CREATE;
  }

  if ((uri[0] == '/') || (uri[0] == '.')) { // TODO make this platform-independent
    // Option 1: Just specify a file name
    return std::unique_ptr<odb::database>(
        new odb::sqlite::database(uri, options));
  }

  const std::regex SQLITE_URI_RE("sqlite://(.*)",
                                 std::regex_constants::ECMAScript);
  std::smatch match_result;
  bool matched = std::regex_match(uri, match_result, SQLITE_URI_RE);

  if (matched) {
    return std::unique_ptr<odb::database>(
        new odb::sqlite::database(match_result[0], options));
  }
#else
  (void)uri;
  (void)rw;
  (void)create;
#endif
  return nullptr;
}

std::unique_ptr<odb::database>
DBFactory::try_get_mysql(std::string uri, bool rw, bool create)
{
  (void)rw;
  (void)create;

#ifdef ODB_MYSQL_FOUND
  const std::regex MYSQL_URI_RE(
      "mysql://([^:]*):([^@]*)@([^:/]*)(:[^/]*)?/(.*)",
      std::regex_constants::ECMAScript);
  std::smatch match_result;
  bool matched = std::regex_match(uri, match_result, MYSQL_URI_RE);

  if (matched) {
    std::string user = match_result[1];
    std::string pwd = match_result[2];
    std::string host = match_result[3];
    std::string port_str = match_result[4];
    std::string db_name = match_result[5];

    unsigned int port = 0;
    if (port_str.size() > 0) {
      port = (unsigned int)std::atoi(port_str.c_str());
    }

    return std::unique_ptr<odb::database>(
        new odb::mysql::database(user, pwd, db_name, host, port));
  }
#else
  (void)uri;
#endif
  
  return nullptr;
}

std::unique_ptr<odb::database>
DBFactory::get(std::string uri, bool rw, bool create)
{
  auto ptr = this->try_get_sqlite(uri, rw, create);
  if (ptr.get() != nullptr) {
    return ptr;
  }
  ptr = this->try_get_mysql(uri, rw, create);
  if (ptr.get() != nullptr) {
    return ptr;
  }

  BOOST_LOG(l.e()) << "Could not decode database URI " << uri;
  throw "Illegal database URI";
}
