//
// Created by lukas on 06.12.17.
//

#include <gtest/gtest.h>

#include "generated_config.hpp"

#include "datastructures/test_skyline.hpp"
#include "instance/test_solution.hpp"
#include "algorithms/test_permutation.hpp"
//#include "state_propagation/test_sp.hpp"
//#include "state_propagation/test_propagator.hpp"
#include "datastructures/test_intrusive_shared_ptr_pool.hpp"
#include "datastructures/test_overlapping_jobs_generator.hpp"

#if defined(GUROBI_FOUND)
#include "ilp/test_ilp.hpp"
#endif

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
