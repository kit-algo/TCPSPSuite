#ifndef COMPLETENESS_CHECKER_H
#define COMPLETENESS_CHECKER_H

#include "../util/configuration.hpp"
#include "../util/log.hpp"
#include "../util/solverconfig.hpp"

class CompletenessChecker {
public:
  CompletenessChecker(bool autotune_mode);
  void run(int argc, const char ** argv);

private:
  void read_instances();
  void generate_scfgs_tcpspsuite();
  void generate_scfgs_autotune();
  void check();

  std::string storage_path;
  std::string config_file;
  std::string instance_dir;
  bool autotune_mode;

  std::vector<SolverConfig> scfgs;
  std::vector<std::string> instances;

  Log l;
};

#endif
