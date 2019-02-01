#ifndef DB_FACTORY_HPP
#define DB_FACTORY_HPP

#include "../util/log.hpp"

#include <string>
#include <memory>

namespace odb {
    class database;
}

class DBFactory {
public:
  DBFactory();

  std::unique_ptr<odb::database> get(std::string uri, bool rw=true, bool create=true);

private:

  std::unique_ptr<odb::database> try_get_sqlite(std::string uri, bool rw, bool create);
  std::unique_ptr<odb::database> try_get_mysql(std::string uri, bool rw, bool create);

  Log l;
};

#endif
