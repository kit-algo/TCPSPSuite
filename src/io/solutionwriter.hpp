#ifndef SOLUTIONWRITER_H
#define SOLUTIONWRITER_H

#include <string>            // for string
#include <json.hpp>  // for json
#include "../datastructures/maybe.hpp"
class Solution;

class SolutionWriter {
public:
  SolutionWriter(const Solution &solution, Maybe<unsigned long int> res_id);
  void write_to(std::string filename);

private:
  void prepare();
  void dump_jobs();

  const Solution &solution;
	Maybe<unsigned long int> res_id;
  nlohmann::json j;
};

#endif
