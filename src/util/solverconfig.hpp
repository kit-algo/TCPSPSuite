#ifndef SOLVERCONFIG_H
#define SOLVERCONFIG_H

#include "../datastructures/maybe.hpp"
#include <json.hpp>

using json = nlohmann::json;
using json_entry = json::basic_json::value_type;

#include <map>
#include <vector>
#include <string>
#include <regex>
#include <fstream>

class SolverConfig {
public:
  SolverConfig();
  SolverConfig(std::string name, std::string id_str,
               std::map<std::string, json_entry> values,
               Maybe<unsigned int> time_limit, Maybe<int> seed);

  const json_entry & operator[](const std::string & key) const;
  bool has_config(const std::string & key) const;
    const std::map<std::string, json_entry> & get_kvs() const;

  const std::string & get_name() const;
  const std::string & get_id() const;
  bool match(std::string match_id) const;
  Maybe<unsigned int> get_time_limit() const;

    bool was_seed_set() const;
    void override_seed(int seed);
    void override_config(const std::string & key, const std::string & value);
  int get_seed() const;

  static std::vector<SolverConfig> read_configs(std::string filename);
  static std::vector<SolverConfig> read_configs(json jsonConfig);

private:
  std::map<std::string, json_entry> values;
  std::string name;

  std::regex id_matcher;
  std::string id_str;
  Maybe<unsigned int> time_limit;
  Maybe<int> seed;
};

#endif
