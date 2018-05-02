#include "solutionwriter.hpp"
#include <fstream>                // for basic_ostream, ofstream, operator<<
#include <iomanip>                // for operator<<, setw
#include "../instance/instance.hpp"  // for Instance
#include "../instance/job.hpp"       // for Job
#include "../instance/solution.hpp"  // for Solution

SolutionWriter::SolutionWriter(const Solution & solution_in, Maybe<unsigned long int> res_id_in)
  : solution(solution_in), res_id(res_id_in)
{
  this->prepare();
  this->dump_jobs();
}

void
SolutionWriter::prepare()
{
  j["instance"] = this->solution.get_instance()->get_id();
  if (this->res_id.valid()) {
    j["res_id"] = this->res_id.value();
  }
}

void
SolutionWriter::dump_jobs()
{
  j["jobs"] = nlohmann::json();

  const Instance & instance = *this->solution.get_instance();

  for (unsigned int jid = 0 ; jid < instance.job_count() ; ++jid) {
    const Job & job = instance.get_job(jid);

    nlohmann::json job_json;
    job_json["id"] = jid;
    job_json["release"] = job.get_release();
    job_json["duration"] = job.get_duration();
    job_json["deadline"] = job.get_deadline();

    job_json["usages"] = nlohmann::json();

    for (unsigned int rid = 0 ; rid < instance.resource_count() ; ++rid) {
      job_json["usages"][std::to_string(rid)] = job.get_resource_usage(rid);
    }

    job_json["start_time"] = this->solution.get_start_time(jid);

    j["jobs"].push_back(job_json);
  }
}

void
SolutionWriter::write_to(std::string filename)
{
  std::ofstream output_file(filename);
  output_file << std::setw(2) << j << "\n";
}
