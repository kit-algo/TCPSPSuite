#ifndef PROGENREADER_HPP
#define PROGENREADER_HPP

#include "../instance/instance.hpp"

#include <string>
#include <memory>

class ProGenReader {
public:
  static const unsigned long int OPT_ONLY_FINISH_START = 1;

  ProGenReader(std::string filename, unsigned long int options = 0);

  Instance *parse();
private:
  std::string filename;
  unsigned long int options;
};

#endif
