#ifndef DOTFILE_H
#define DOTFILE_H

#include <iostream>  // for ostringstream
#include <string>    // for string
#include <sstream>

class Instance;

class DotfileExporter {
public:
  DotfileExporter(const Instance & instance);

  void write(std::string filename);

private:
  void add_header();
  void add_footer();
  void add_job(unsigned int job_id);
  void add_edge(unsigned int job_from, unsigned int job_to);
  void prepare();

  std::ostringstream buf;

  const Instance &instance;
};

#endif
