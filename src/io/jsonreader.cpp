#include "jsonreader.hpp"
#include <ext/alloc_traits.h>        // for __alloc_traits<>::value_type
#include <algorithm>                 // for move, sort
#include <fstream>                   // for stringstream, basic_ostream::ope...
#include <utility>                   // for make_pair
#include <vector>                    // for vector
#include "../instance/instance.hpp"  // for Instance
#include "../instance/job.hpp"       // for Job
#include "../instance/laggraph.hpp"  // for LagGraph
#include "../instance/resource.hpp"  // for Resource, polynomial
#include "../instance/traits.hpp"    // for Traits
#include "../util/log.hpp"           // for Log

using json = nlohmann::json;

JsonMalformedException::JsonMalformedException(const char *what) :
  std::runtime_error(what)
{}

JsonReader::JsonReader(std::string filename_in) :
  filename(filename_in), l("JSONREADER")
{}

Instance *
JsonReader::parse()
{
  std::ifstream in_stream(this->filename);
  std::stringstream buffer;
  buffer << in_stream.rdbuf();
  BOOST_LOG(l.d()) << "Parsing " << this->filename ;
  this->js = json::parse(buffer.str());

  Instance *instance = new Instance(this->get_json<std::string>("id"), Traits());

  std::vector<json> job_data;
  for (json one_job : js["jobs"]) {
    job_data.push_back(one_job);
  }
  std::sort(job_data.begin(), job_data.end(), [&](const json &a, const json &b) {
    return a["id"] < b["id"];
  });

  //std::cout << "Sorted jobs\n";

  for (unsigned int i = 0 ; i < js["jobs"].size() ; i++) {
    // TODO this is actually pretty ugly..
    if (((unsigned int)job_data[i]["id"]) != i) {
      throw JsonMalformedException("Job IDs must be sequential.");
    }

    std::vector<double> usages;
    for (unsigned int j = 0 ; j < js["resources"].size() ; j++) {
      usages.push_back(job_data[i]["usages"][std::to_string(j)]);
    }

    // Dummy ID 0 will be set by add_job
    Job job(job_data[i]["release"], job_data[i]["deadline"], job_data[i]["duration"], usages, 0);
    instance->add_job(std::move(job));
  }

  //std::cout << "Added jobs\n";

  for (unsigned int i = 0 ; i < js["jobs"].size() ; i++) {
    const Job &job_from = instance->get_job(i);

    for (json::iterator it = job_data[i]["successors"].begin(); it != job_data[i]["successors"].end(); ++it) {
      const Job & job_to = instance->get_job((unsigned int)std::stoi(it.key()));

      const auto & edge_data = it.value();

      instance->get_laggraph().add_edge(job_from, job_to, {edge_data.at("lag"), edge_data.at("drain_factor"), edge_data.at("max_recharge")});
    }
  }

  //std::cout << "Added Lags\n";

  std::vector<json> resource_data;
  for (json one_resource : js["resources"]) {
    resource_data.push_back(one_resource);
  }
  for (unsigned int i = 0 ; i < js["resources"].size() ; i++) {
    // TODO this is actually pretty ugly..
    if (((unsigned int)resource_data[i]["id"]) != i) {
      throw JsonMalformedException("Resource IDs must be sequential.");
    }

    polynomial overshoot_costs, investment_costs;
    for (auto term : resource_data[i]["overshoot_costs"]) {
      overshoot_costs.push_back(std::make_pair(term[0], term[1]));
    }
    for (auto term : resource_data[i]["investment_costs"]) {
      investment_costs.push_back(std::make_pair(term[0], term[1]));
    }
    Resource res(i);
    res.set_overshoot_costs(overshoot_costs);
    res.set_investment_costs(investment_costs);

	  if (resource_data[i].find("availability") != resource_data[i].end()) {
		  // Profile given
		  std::vector<std::pair<unsigned int, double>> points;
		  for (json point_data : resource_data[i]["availability"]) {
			  points.push_back({point_data[0], point_data[1]});
		  }
		  assert(points.size() > 0);
		  instance::Availability av(0);
		  av.set(std::move(points));
		  res.set_availability(std::move(av));
	  } else if (resource_data[i].find("free_amount") != resource_data[i].end()) {
		  // Flat resource availability
		  res.set_availability(instance::Availability((double)resource_data[i]["free_amount"]));
	  }

    instance->add_resource(std::move(res));
  }

  if (js.find("window_extension") != js.end()) {
    auto we = js["window_extension"];
    unsigned int window_extension_limit = this->get_json<unsigned int, decltype(we)>
                                                      ("time_limit", we);
    unsigned int window_extension_job_limit = this->get_json<unsigned int, decltype(we)>
                                                          ("job_limit", we);
    instance->set_window_extension(window_extension_limit, window_extension_job_limit);
  }

  return instance;
}
