#include "progenreader.hpp"

#include <fstream>
#include <iostream>

ProGenReader::ProGenReader(std::string filename_in, unsigned long int options_in) :
  filename(filename_in), options(options_in)
{}

Instance *
ProGenReader::parse()
{

  std::ifstream infile(this->filename);

  // Read header
  int njobs, nres, dummy1, dummy2, deadline;
  infile >> njobs >> nres >> dummy1 >> dummy2 >> deadline;
  njobs += 2; // They have a very weird file format...

  // Create instance
  Instance *instance = new Instance(Instance::INSTANCE_TCPSP, Instance::LIMITATION_ALIGNED_STARTEND);
  LagGraph *laggraph = instance->get_laggraph();

  // Collect entries into the lag graph only, since depending on options,
  // the lag might be determined by the job duration.
  std::vector<std::tuple<int, int, int>> lags;

  for (int job_id = 0 ; job_id < njobs ; job_id++) {
    int job_id_dummy, one_dummy, successor_count;

    infile >> job_id_dummy >> one_dummy >> successor_count;
    if (job_id_dummy != job_id) {
      std::cout << "Job IDs: " << job_id_dummy << " vs " << job_id << "\n" << std::flush;
      throw "Job IDs inconsistent while reading graph.";
    }

    if (one_dummy != 1) {
      throw "Expected a one. File inconsistent.";
    }

    std::vector<int> these_successors;
    for (int i = 0 ; i < successor_count ; i++) {
      int successor_id;
      infile >> successor_id;
      std::cout << "Pushing successor for " << job_id << ": " << successor_id << "\n";
      these_successors.push_back(successor_id);
    }

    for (int i = 0 ; i < successor_count ; i++) {
      char brace;
      int lag;

      infile >> brace;
      if (brace != '[') {
        throw "Expected [. File inconsistent.";
      }
      infile >> lag;
      infile >> brace;
      if (brace != ']') {
        throw "Expected ]. File inconsistent.";
      }
      //laggraph->add_edge(job_id, these_successors[i], {lag});

      std::cout << "Pushing lag for " << job_id << " and successor" << these_successors[i] << ": " << lag << "\n";

      lags.push_back(std::make_tuple(job_id, these_successors[i], lag));
    }
  }

  std::cout << "Lags done.\n";

  // Now there must be the line separating successor-list from duration / usage - list
  // THIS IS ACTUALLY A JOB?!?
  /*
  infile >> dummy1 >> dummy2 >> dummy3;
  if ((dummy1 != njobs) || (dummy2 != 1) || (dummy3 != 0)) {
    throw "First separation Line not present. File inconsistent.";
  }
  */

  // Now, read durations and usages.
  for (int job_id = 0 ; job_id < njobs ; job_id++) {
    int job_id_dummy, one_dummy;
    int duration;
    infile >> job_id_dummy >> one_dummy >> duration;

    if (job_id_dummy != job_id) {
      std::cout << "Job IDs: " << job_id_dummy << " vs " << job_id << "\n" << std::flush;
      throw "Job IDs inconsistent while reading jobs.";
    }

    if (one_dummy != 1) {
      std::cout << "One-dummy was: " << one_dummy << "\n";
      throw "Expected a one. File inconsistent.";
    }

    std::vector<double> usages;
    for (int i = 0 ; i < nres ; i++) {
      double usage;
      infile >> usage;
      usages.push_back(usage);
    }

    std::cout << "Trying to create a job.." << std::flush;
    std::unique_ptr<Job> job(new Job(0, deadline, duration, usages, job_id));
    std::cout << "..done. Trying to add the job..." << std::flush;
    instance->add_job(std::move(job));
    std::cout << "..done\n";
  }

  // Now, actually create lags
  std::cout << "Lags are: \n";
  for (auto lag : lags) {
    std::cout << std::get<0>(lag) << " -> " << std::get<1>(lag) << ": " << std::get<2>(lag) << "\n";
  }

  for (auto lag : lags) {
    int job_from, job_to;
    int lag_amount;
    std::tie(job_from, job_to, lag_amount) = lag;
    const Job *start_job = instance->get_job(job_from);
    const Job *end_job = instance->get_job(job_to);

    if (this->options & OPT_ONLY_FINISH_START) {
      LagGraph::edge e;
      e.lag = start_job->get_duration();
      std::cout << "Adding edge from " << job_from << " to " << job_to << "\n";
      laggraph->add_edge(start_job, end_job, e);
    } else {
      laggraph->add_edge(start_job, end_job, {lag_amount});
    }
  }

  // Create resources
  for (int res_id = 0; res_id < nres ; res_id++) {
    double cost_coefficient;
    polynomial costs = {{cost_coefficient, 1}};
    std::unique_ptr<Resource> resource(new InvestmentResource(costs, res_id));
    instance->add_resource(std::move(resource));
  }

  return instance;
}
