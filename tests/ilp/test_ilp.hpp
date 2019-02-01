//
// Created by lukas on 07.12.17.
//

#ifndef TCPSPSUITE_TEST_ILP_HPP
#define TCPSPSUITE_TEST_ILP_HPP

#include <random>

using namespace testing;

#include "../src/contrib/ilpabstraction/src/ilpa_gurobi.hpp"
#include "../src/db/storage.hpp"
#include "../src/ilp/dtilp.hpp"
#include "../src/ilp/ebilp.hpp"
#include "../src/instance/laggraph.hpp"
#include "../src/manager/errors.hpp"
#include "../src/util/solverconfig.hpp"

namespace test {
namespace ilp {

constexpr unsigned int TEST_SEED = 4;

class ILPTest : public Test {
public:
  ILPTest() : sconf("GRBTest", "ID", {}, {10u}, false, 1, {}, {1}) {}

  virtual void
  SetUp()
  {
    instance = Instance();
    rng = std::mt19937(TEST_SEED);
  }

  void
  run_gurobi_dtilp()
  {
    AdditionalResultStorage aresult;
    DTILP<ilpabstraction::GurobiInterface> ilp(instance, aresult, sconf);
    ilp.run();
    sol = ilp.get_solution();
  }

  void
  run_gurobi_ebilp()
  {
    AdditionalResultStorage aresult;
    EBILP<ilpabstraction::GurobiInterface> ilp(instance, aresult, sconf);
    ilp.run();
    sol = ilp.get_solution();
  }

  SolverConfig sconf;
  Solution sol;
  Instance instance;
  std::mt19937 rng;
};

TEST_F(ILPTest, BasicTest)
{
  Resource res(0);
  res.set_investment_costs({{1.0, 1.0}});

  this->instance.add_resource(std::move(res));

  Job j0(0, 10, 5, {1.0}, 0);
  Job j1(0, 10, 5, {1.0}, 1);
  Job j2(9, 10, 1, {2.0}, 2);

  Job j3(100, 200, 10, {1.0}, 3);
  Job j4(100, 200, 10, {1.0}, 4);
  Job j5(100, 101, 1, {1.0}, 5);
  Job j6(199, 200, 1, {3.0}, 6);

  instance.add_job(std::move(j0));
  instance.add_job(std::move(j1));
  instance.add_job(std::move(j2));
  instance.add_job(std::move(j3));
  instance.add_job(std::move(j4));
  instance.add_job(std::move(j5));
  instance.add_job(std::move(j6));

  instance.get_laggraph().add_edge(instance.get_job(0), instance.get_job(1),
                                   {5, 0, 0});
  instance.get_laggraph().add_edge(instance.get_job(3), instance.get_job(4),
                                   {89, 0, 0});

  this->run_gurobi_dtilp();

  this->sol.verify(sconf.get_seed());

  ASSERT_EQ(sol.get_start_time(0), 0);
  ASSERT_EQ(sol.get_start_time(1), 5);
  ASSERT_EQ(sol.get_start_time(2), 9);

  ASSERT_EQ(sol.get_start_time(3), 100);
  ASSERT_EQ(sol.get_start_time(4), 189);
  ASSERT_EQ(sol.get_start_time(5), 100);
  ASSERT_EQ(sol.get_start_time(6), 199);

  ASSERT_EQ(sol.get_max_usage(0), 3.0);
}

TEST_F(ILPTest, TestWindowExtensionViolation)
{
  Resource res(0);
  res.set_investment_costs({{1.0, 1.0}});

  this->instance.add_resource(std::move(res));

  Job j0(10, 20, 6, {1.0}, 0);
  Job j1(10, 20, 6, {1.0}, 1);
  Job j2(10, 20, 0, {1.0}, 2);

  instance.add_job(std::move(j0));
  instance.add_job(std::move(j1));
  instance.add_job(std::move(j2));

  instance.set_window_extension(2, 2);

  this->sconf.override_seed(5);
  this->run_gurobi_dtilp();

  unsigned int start0 = this->sol.get_start_time(0);
  unsigned int start1 = this->sol.get_start_time(1);
  unsigned int start2 = this->sol.get_start_time(2);

  unsigned int window_extension = 0;
  window_extension += (unsigned int)std::max(0, 10 - (int)start0);
  window_extension += (unsigned int)std::max(0, 10 - (int)start1);
  window_extension += (unsigned int)std::max(0, 10 - (int)start2);

  window_extension += (unsigned int)std::max(0, (int)start0 - 15);
  window_extension += (unsigned int)std::max(0, (int)start1 - 15);
  window_extension += (unsigned int)std::max(0, (int)start1 - 20);

  this->sol.verify(sconf.get_seed());

  ASSERT_GE(start2, 10);
  ASSERT_LE(start2, 20);

  ASSERT_EQ(this->sol.get_costs(), 1.0);

  ASSERT_LE(window_extension, 2);
}

TEST_F(ILPTest, ProvokeOffByOne)
{
  Resource res(0);
  res.set_investment_costs({{1.0, 1.0}});

  this->instance.add_resource(std::move(res));

  Job j0(10, 12, 2, {1.0}, 0);
  Job j1(10, 12, 2, {1.0}, 1);
  Job j2(11, 13, 2, {1.0}, 2);
  Job j3(11, 13, 2, {1.0}, 3);

  instance.add_job(std::move(j0));
  instance.add_job(std::move(j1));
  instance.add_job(std::move(j2));
  instance.add_job(std::move(j3));

  instance.set_window_extension(1, 1);

  this->sconf.override_seed(5);

  this->run_gurobi_dtilp();

  unsigned int start0 = this->sol.get_start_time(0);
  unsigned int start1 = this->sol.get_start_time(1);
  unsigned int start2 = this->sol.get_start_time(2);
  unsigned int start3 = this->sol.get_start_time(3);

  unsigned int window_extension = 0;
  window_extension += (unsigned int)std::max(0, 10 - (int)start0);
  window_extension += (unsigned int)std::max(0, 10 - (int)start1);
  window_extension += (unsigned int)std::max(0, 11 - (int)start2);
  window_extension += (unsigned int)std::max(0, 11 - (int)start3);

  window_extension += (unsigned int)std::max(0, (int)start0 - 10);
  window_extension += (unsigned int)std::max(0, (int)start1 - 10);
  window_extension += (unsigned int)std::max(0, (int)start2 - 11);
  window_extension += (unsigned int)std::max(0, (int)start3 - 11);

  this->sol.verify(sconf.get_seed());

  ASSERT_EQ(this->sol.get_costs(), 3.0);

  ASSERT_LE(window_extension, 1);

  this->run_gurobi_ebilp();

  start0 = this->sol.get_start_time(0);
  start1 = this->sol.get_start_time(1);
  start2 = this->sol.get_start_time(2);
  start3 = this->sol.get_start_time(3);

  window_extension = 0;
  window_extension += (unsigned int)std::max(0, 10 - (int)start0);
  window_extension += (unsigned int)std::max(0, 10 - (int)start1);
  window_extension += (unsigned int)std::max(0, 11 - (int)start2);
  window_extension += (unsigned int)std::max(0, 11 - (int)start3);

  window_extension += (unsigned int)std::max(0, (int)start0 - 10);
  window_extension += (unsigned int)std::max(0, (int)start1 - 10);
  window_extension += (unsigned int)std::max(0, (int)start2 - 11);
  window_extension += (unsigned int)std::max(0, (int)start3 - 11);

  this->sol.verify(sconf.get_seed());

  ASSERT_EQ(this->sol.get_costs(), 3.0);

  ASSERT_LE(window_extension, 1);
}

TEST_F(ILPTest, BasicExtensionTest)
{
  Resource res(0);
  res.set_investment_costs({{1.0, 1.0}});

  this->instance.add_resource(std::move(res));

  Job j0(10, 20, 10, {2.0}, 0);
  Job j1(14, 30, 16, {1.0}, 1);
  Job j2(15, 30, 15, {1.0}, 2);
  Job j3(15, 30, 15, {1.0}, 3);

  instance.add_job(std::move(j0));
  instance.add_job(std::move(j1));
  instance.add_job(std::move(j2));
  instance.add_job(std::move(j3));

  instance.set_window_extension(5, 1);

  this->run_gurobi_dtilp();

  this->sol.verify(sconf.get_seed());

  ASSERT_EQ(sol.get_max_usage(0), 3.0);

  ASSERT_EQ(sol.get_start_time(0), 5);
  ASSERT_EQ(sol.get_start_time(1), 14);
  ASSERT_EQ(sol.get_start_time(2), 15);
  ASSERT_EQ(sol.get_start_time(3), 15);
}

TEST_F(ILPTest, CanExtendRightTest)
{
  Resource res(0);
  res.set_investment_costs({{1.0, 1.0}});

  this->instance.add_resource(std::move(res));

  Job j0(0, 5, 5, {1.0}, 0);
  Job j1(2, 5, 2, {1.0}, 1);

  instance.add_job(std::move(j0));
  instance.add_job(std::move(j1));

  instance.set_window_extension(2, 1);

  this->run_gurobi_dtilp();

  this->sol.verify(sconf.get_seed());

  ASSERT_EQ(sol.get_max_usage(0), 1.0);

  ASSERT_EQ(sol.get_start_time(0), 0);
  ASSERT_EQ(sol.get_start_time(1), 5);
}

TEST_F(ILPTest, CanExtendLeftTest)
{
  Resource res(0);
  res.set_investment_costs({{1.0, 1.0}});

  this->instance.add_resource(std::move(res));

  Job j0(10, 15, 5, {2.0}, 0);
  Job j1(10, 20, 10, {1.0}, 1);
  Job j2(10, 20, 10, {1.0}, 2);

  instance.add_job(std::move(j0));
  instance.add_job(std::move(j1));
  instance.add_job(std::move(j2));

  instance.set_window_extension(5, 1);

  this->run_gurobi_dtilp();

  this->sol.verify(sconf.get_seed());

  ASSERT_EQ(sol.get_max_usage(0), 2.0);

  ASSERT_EQ(sol.get_start_time(0), 5);
  ASSERT_EQ(sol.get_start_time(1), 10);
  ASSERT_EQ(sol.get_start_time(2), 10);
}

TEST_F(ILPTest, TestOvershootWithChangingAvailability)
{
  Availability av(0);
  av.set({{0, 5.0}, {10, 10.0}, {20, 15.0}, {30, 0.0}});
  Resource res(0);
  res.set_availability(std::move(av));
  res.set_overshoot_costs({
      {1.0, 1.0},
  });

  instance.add_resource(std::move(res));

  // Job that needs to be executed from 0 to 30. Overshoots during the first 10
  // time steps by 1.
  Job j0(0, 100, 30, {6.0}, 0);
  instance.add_job(std::move(j0));

  // Job that needs to be executed between 20 and 30
  Job j1(0, 100, 10, {9.0}, 1);
  instance.add_job(std::move(j1));

  this->run_gurobi_dtilp();
  this->sol.verify(sconf.get_seed());

  ASSERT_EQ(sol.get_start_time(0), 0);
  ASSERT_EQ(sol.get_start_time(1), 20);
  ASSERT_EQ(sol.get_costs(), 10);
}

TEST_F(ILPTest, TestInvestmentWithChangingAvailability)
{
  Availability av(0);
  av.set({{0, 5.0}, {10, 10.0}, {20, 15.0}, {21, 0.0}});
  Resource res(0);
  res.set_availability(std::move(av));
  res.set_investment_costs({
      {1.0, 1.0},
  });

  instance.add_resource(std::move(res));

  // Job that needs to be executed from 10 to 20. Overshoots during the 10 time
  // steps by 1.
  Job j0(0, 20, 10, {11.0}, 0);
  instance.add_job(std::move(j0));

  // Job that needs to be executed between 20 and 21. Overshoots by 1, too.
  Job j1(0, 100, 1, {16.0}, 1);
  instance.add_job(std::move(j1));

  this->run_gurobi_dtilp();
  this->sol.verify(sconf.get_seed());

  ASSERT_EQ(sol.get_start_time(0), 10);
  ASSERT_EQ(sol.get_start_time(1), 20);
  ASSERT_EQ(sol.get_costs(), 1);
}

} // namespace ilp
} // namespace test

#endif // TCPSPSUITE_TEST_ILP_HPP
