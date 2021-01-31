#include "jsonreader.hpp"

#include "../instance/instance.hpp" // for Instance
#include "../instance/job.hpp"      // for Job
#include "../instance/laggraph.hpp" // for LagGraph
#include "../instance/resource.hpp" // for Resource, polynomial
#include "../instance/traits.hpp"   // for Traits
#include "../util/log.hpp"          // for Log

#include <algorithm>          // for move, sort
#include <ext/alloc_traits.h> // for __alloc_traits<>::value_type
#include <fstream>            // for stringstream, basic_ostream::ope...
#include <utility>            // for make_pair
#include <vector>             // for vector

using json = nlohmann::json;

InstanceMalformedException::InstanceMalformedException(const char * what)
    : std::runtime_error(what)
{}

JsonReader::JsonReader(std::string filename_in)
    : filename(filename_in), l("JSONREADER")
{}

Instance *
JsonReader::parse()
{
	std::ifstream in_stream(this->filename);
	std::stringstream buffer;
	buffer << in_stream.rdbuf();
	BOOST_LOG(l.d()) << "Parsing " << this->filename;
	this->js = json::parse(buffer.str());

	this->instance = new Instance(this->get_json<std::string>("id"), Traits());

	this->parse_resources();
	this->parse_jobs();

	// Transferring ownership
	return this->instance;
}

void
JsonReader::parse_resources()
{
	std::vector<json> resource_data;
	for (const json & one_resource : js.at("resources")) {
		resource_data.push_back(one_resource);
	}
	std::sort(resource_data.begin(), resource_data.end(),
	          [&](const json & a, const json & b) { return a["id"] < b["id"]; });

	for (unsigned int i = 0; i < js["resources"].size(); i++) {
		if ((get_json<unsigned int>("id", resource_data[i])) != i) {
			throw InstanceMalformedException("Resource IDs must be consecutive.");
		}

		polynomial overshoot_costs_base, investment_costs;
		for (auto term : resource_data[i].at("overshoot_costs")) {
			overshoot_costs_base.push_back(std::make_pair(term[0], term[1]));
		}
		for (auto term : resource_data[i].at("investment_costs")) {
			investment_costs.push_back(std::make_pair(term[0], term[1]));
		}

		if (overshoot_costs_base.empty() && investment_costs.empty()) {
			BOOST_LOG(l.w()) << "Resource " << i << " has no associated costs.";
		}

		FlexCost overshoot_cost(overshoot_costs_base);

		if (resource_data[i].find("flex_overshoot_costs") !=
		    resource_data[i].end()) {
			std::vector<std::pair<unsigned int, polynomial>> points;
			for (json point_data : resource_data[i]["flex_overshoot_costs"]) {
				polynomial costs;
				for (auto term : point_data[1]) {
					costs.push_back(std::make_pair(term[0], term[1]));
				}

				points.push_back({point_data[0], costs});
			}

			overshoot_cost.set_flexible(std::move(points));
		}

		Resource res(i);
		res.set_overshoot_costs(std::move(overshoot_cost));
		res.set_investment_costs(investment_costs);

		if (resource_data[i].find("availability") != resource_data[i].end()) {
			// Profile given
			std::vector<std::pair<unsigned int, double>> points;
			for (json point_data : resource_data[i]["availability"]) {
				points.push_back({point_data[0], point_data[1]});
			}
			assert(points.size() > 0);
			Availability av(0);
			av.set(std::move(points));
			res.set_availability(std::move(av));
		} else if (resource_data[i].find("free_amount") != resource_data[i].end()) {
			// Flat resource availability
			res.set_availability(
			    Availability((double)resource_data[i]["free_amount"]));
		}

		instance->add_resource(std::move(res));
	}
}

void
JsonReader::parse_jobs()
{
	std::vector<json> job_data;
	for (json one_job : js.at("jobs")) {
		job_data.push_back(one_job);
	}
	std::sort(job_data.begin(), job_data.end(),
	          [&](const json & a, const json & b) { return a["id"] < b["id"]; });

	// std::cout << "Sorted jobs\n";

	for (unsigned int i = 0; i < js["jobs"].size(); i++) {
		if (((unsigned int)job_data[i].at("id")) != i) {
			throw InstanceMalformedException("Job IDs must be consecutive.");
		}

		ResVec usages(this->instance->resource_count(), 0.0);
		for (auto it = job_data[i].at("usages").begin();
		     it != job_data[i].at("usages").end(); ++it) {
			size_t resource_index = static_cast<size_t>(std::stoi(it.key()));
			if (resource_index >= this->instance->resource_count()) {
				throw InstanceMalformedException(
				    "Invalid resource in job specification.");
			}
			usages[resource_index] = it.value();
		}

		// Dummy ID 0 will be set by add_job
		Job job(job_data[i].at("release"), job_data[i].at("deadline"),
		        job_data[i].at("duration"), usages, 0);

		if (job_data[i].find("hint") != job_data[i].end()) {
			job.set_hint(static_cast<unsigned int>(job_data[i]["hint"]));
		}

		instance->add_job(std::move(job));
	}

	// std::cout << "Added jobs\n";

	for (unsigned int i = 0; i < js["jobs"].size(); i++) {
		const Job & job_from = instance->get_job(i);

		for (json::iterator it = job_data[i].at("successors").begin();
		     it != job_data[i]["successors"].end(); ++it) {
			const Job & job_to = instance->get_job((unsigned int)std::stoi(it.key()));

			const auto & edge_data = it.value();

			instance->get_laggraph().add_edge(job_from, job_to,
			                                  {edge_data.at("lag"),
			                                   edge_data.at("drain_factor"),
			                                   edge_data.at("max_recharge")});
		}
	}

	// std::cout << "Added Lags\n";

	if (js.find("window_extension") != js.end()) {
		auto we = js["window_extension"];
		unsigned int window_extension_limit =
		    this->get_json<unsigned int, decltype(we)>("time_limit", we);
		unsigned int window_extension_job_limit =
		    this->get_json<unsigned int, decltype(we)>("job_limit", we);
		instance->set_window_extension(window_extension_limit,
		                               window_extension_job_limit);

		Maybe<unsigned int> hard_deadline;
		if (we.find("hard_deadline") != we.end()) {
			hard_deadline =
			    this->get_json<unsigned int, decltype(we)>("hard_deadline", we);
		}
		instance->set_window_extension_hard_deadline(hard_deadline);
	}
}
