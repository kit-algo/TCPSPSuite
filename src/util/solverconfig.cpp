#include "solverconfig.hpp"

SolverConfig::SolverConfig()
{}

SolverConfig::SolverConfig(std::string name_in, std::string id_str_in,
                           std::map<std::string, json::basic_json::value_type> values_in,
                           Maybe<unsigned int> time_limit_in, Maybe<int> seed_in)
  : values(values_in), name(name_in), id_matcher(id_str_in), id_str(id_str_in),
    time_limit(time_limit_in), seed(seed_in)
{}

const json::basic_json::value_type &
SolverConfig::operator[](const std::string & key) const
{
  return this->values.at(key);
}

const std::map<std::string, json_entry> &
SolverConfig::get_kvs() const
{
	return this->values;
}

bool
SolverConfig::has_config(const std::string & key) const
{
  return this->values.find(key) != this->values.end();
}

void
SolverConfig::override_config(const std::string & key, const std::string & value)
{
	this->values[key] = value;
}

const std::string &
SolverConfig::get_name() const
{
  return this->name;
}

const std::string &
SolverConfig::get_id() const
{
  return this->id_str;
}

bool
SolverConfig::match(const std::string match_id) const
{
  return std::regex_match(match_id, this->id_matcher);
}

Maybe<unsigned int>
SolverConfig::get_time_limit() const
{
  return this->time_limit;
}

bool
SolverConfig::was_seed_set() const
{
	return this->seed.valid();
}

void
SolverConfig::override_seed(int seed_in)
{
	this->seed = seed_in;
}

int
SolverConfig::get_seed() const
{
	if (!this->seed.valid()) {
		throw "Seed was not set!";
	}

  return this->seed.value();
}

std::vector<SolverConfig>
SolverConfig::read_configs(std::string filename) {
  std::ifstream in_stream(filename);
  std::stringstream buffer;
  buffer << in_stream.rdbuf();

  return read_configs(json::parse(buffer.str()));
}

std::vector<SolverConfig>
SolverConfig::read_configs(json js) {
  std::vector<SolverConfig> cfgs;

  for (const auto & entry : js["solvers"]) {
    std::string id_str = entry["regex"];
    std::string name_str = entry["name"];

    std::map<std::string, json::basic_json::value_type> values;

	  Maybe<unsigned int> time_limit;
	  if (entry.find("time_limit") != entry.end()) {
		  std::string time_limit_str = entry["time_limit"];
			time_limit = (unsigned int)std::stoul(time_limit_str);
	  }

    if ((entry.find("config") != entry.end()) && (entry["config"].size() > 0)) {
      for (auto element_it = entry["config"].begin() ; element_it != entry["config"].end() ; ++element_it) {
        values[element_it.key()] = element_it.value();
      }
    }

    SolverConfig sc { name_str, id_str, values, time_limit, Maybe<int>()};
    if (entry.find("seed") != entry.end()) {
      std::string seed_str = entry["seed"];
      sc.seed = Maybe<int>(std::stoi(seed_str));
    }
    cfgs.push_back(sc);
  }

  return cfgs;
}
