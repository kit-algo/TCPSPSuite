#ifndef SOLVERCONFIG_H
#define SOLVERCONFIG_H

#include "../datastructures/maybe.hpp"
#include "../util/log.hpp"

#include <json.hpp>

using json = nlohmann::json;
using json_entry = json::basic_json::value_type;

#include <fstream>
#include <map>
#include <regex>
#include <string>
#include <unordered_set>
#include <vector>

class SolverConfig {
public:
	SolverConfig();
	SolverConfig(std::string name, std::string id_str,
	             std::map<std::string, json_entry> values,
	             Maybe<unsigned int> time_limit, bool enable_memory_metrics,
	             unsigned int meminfo_sampling_time,
	             std::vector<std::string> papi_metrics, Maybe<int> seed);

	const json_entry & operator[](const std::string & key) const;
	bool has_config(const std::string & key) const;
	const std::map<std::string, json_entry> & get_kvs() const;
	bool as_bool(const std::string & key) const;

	const std::string & get_name() const;
	const std::string & get_id() const;
	bool match(std::string match_id) const;
	Maybe<unsigned int> get_time_limit() const;

	bool are_memory_metrics_enabled() const;
	unsigned int get_meminfo_sampling_time() const;
	const std::vector<std::string> & get_papi_metrics() const;

	bool was_seed_set() const;
	void override_seed(int seed);
	void override_config(const std::string & key, const std::string & value);
	int get_seed() const;

	void check_all_keys_queried() const;

	static std::vector<SolverConfig> read_configs(const std::string & filename);
	static std::vector<SolverConfig> read_configs(json jsonConfig);

private:
	std::map<std::string, json_entry> values;
	// TODO this is unclean
	mutable std::unordered_set<std::string> requested_keys;
	std::string name;

	std::regex id_matcher;
	std::string id_str;
	Maybe<unsigned int> time_limit;

	bool enable_memory_metrics;
	unsigned int meminfo_sampling_time;
	std::vector<std::string> papi_metrics;

	Maybe<int> seed;

	std::string dbg_serialization;

	Log l;
};

/*
 * Hashing is needed for a stable order inside the partitions in the
 * Parallelizer!
 */
namespace std {
template <>
struct hash<SolverConfig>
{
	size_t
	operator()(const SolverConfig & sc) const noexcept
	{
		size_t hashval = std::hash<std::string>{}(sc.get_name());
		hashval ^= std::hash<std::string>{}(sc.get_id());
		if (sc.get_time_limit().valid()) {
			hashval ^= std::hash<unsigned int>{}(sc.get_time_limit().value());
		}
		// TODO should the seed be hashed?
		//    hash ^= std::hash<int>{}(sc.get_seed());

		for (const auto & kv_entry : sc.get_kvs()) {
			hashval ^= (std::hash<std::string>{}(kv_entry.first) *
			            std::hash<std::string>{}(kv_entry.second.dump()));
		}

		return hashval;
	}
};
} // namespace std

#endif
