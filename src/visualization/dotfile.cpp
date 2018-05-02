#include "dotfile.hpp"
#include <fstream>
#include "../instance/instance.hpp"  // for Instance
#include "../instance/job.hpp"       // for Job
#include "../instance/laggraph.hpp"

DotfileExporter::DotfileExporter(const Instance & instance_in)
  : instance(instance_in)
{}

void
DotfileExporter::add_header()
{
  this->buf << "digraph G {\n";
}

void
DotfileExporter::write(std::string filename)
{
  if (this->buf.tellp() < 1) {
    this->prepare();
  }

  std::ofstream outfile;
  outfile.open(filename);

  outfile << this->buf.str();

  outfile.close();
}

void
DotfileExporter::add_job(unsigned int job_id) {
  const Job & job = this->instance.get_job(job_id);

  this->buf << "  " << job_id << " [label=\"" << job_id << " / " << job.get_duration() << " @ " << job.get_release() << " -> " << job.get_deadline() << "\"]\n";
}

void
DotfileExporter::add_edge(unsigned int job_from, unsigned int job_to) {
  const Job & from = this->instance.get_job(job_from);
  const Job & to = this->instance.get_job(job_to);

  auto edge = this->instance.get_laggraph().get_edge(from, to);

  this->buf << "  " << job_from << " -> " << job_to << " [label=\"" << edge->lag << "\"]\n";
}

void
DotfileExporter::prepare()
{
  this->add_header();

  for (unsigned int j = 0 ; j < this->instance.job_count() ; ++j) {
    this->add_job(j);
  }

  for (auto edge : this->instance.get_laggraph().edges()) {
    this->add_edge(edge.s, edge.t);
  }

  this->add_footer();
}

void
DotfileExporter::add_footer()
{
  this->buf << "}\n";
}
