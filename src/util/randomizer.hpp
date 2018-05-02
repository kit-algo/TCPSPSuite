#ifndef RANDOMIZER_H
#define RANDOMIZER_H

#include <mutex>
#include <random>
#include "datastructures/maybe.hpp"

class Randomizer {
public:
  Randomizer(Maybe<int> global_seed);
  int get_random();
  int get_global_seed();

private:
    std::mutex lock;

  int global_seed;
  std::mt19937 rng;
  std::uniform_int_distribution<int> uni;
};

#endif
