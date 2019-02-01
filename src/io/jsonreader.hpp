#ifndef JSONREADER_H
#define JSONREADER_H

#include <stdexcept>                                    // for domain_error
#include <string>                                       // for streamsize
#include <json.hpp>                             // for json
#include "../util/log.hpp"                                 // for Log
class Instance;

class JsonMalformedException : public std::runtime_error
{
public:
  explicit JsonMalformedException(const char *what);
};

class JsonReader {
public:
  JsonReader(std::string filename);

  Instance *parse();
private:
  std::string filename;
  nlohmann::json js;

  template<class T>
  T get_json(const char * key) {
    try {
      return this->js[key];
    } catch (const std::domain_error & e) {
      BOOST_LOG(l.e()) << "Got an error trying to access " << key ;
      throw e;
    }
  }

  template<class T, class json_T>
  T get_json(const char * key, json_T & js_in) {
    try {
      return js_in[key];
    } catch (const std::domain_error & e) {
      BOOST_LOG(l.e()) << "Got an error trying to access " << key ;
      throw e;
    }
  }

  Log l;
};

#endif
