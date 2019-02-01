//
// Created by lukas on 06.12.17.
//

#ifndef TCPSPSUITE_TEST_SKYLINE_HPP
#define TCPSPSUITE_TEST_SKYLINE_HPP

#include <random>

using namespace testing;

#include "../src/datastructures/skyline.hpp"
#include "../src/instance/resource.hpp"
#include "../src/instance/instance.hpp"

namespace test {
namespace skyline {

constexpr unsigned int TEST_SEED = 4;
constexpr unsigned int TEST_JOBCOUNT = 200;

/*
 * Tree based SkyLine
 */

class TreeSkyLineTest : public Test {
public:
  virtual void
  SetUp()
  {
    rng = std::mt19937(TEST_SEED);
    Resource res(0);
    res.set_investment_costs({{1.0, 1.0}});

    this->instance.add_resource(std::move(res));

    std::uniform_int_distribution<unsigned int> release_dist(
        0, 10 * TEST_JOBCOUNT);
    std::normal_distribution<> slack_dist(50, 20);
    std::normal_distribution<> duration_dist(30, 20);
    std::normal_distribution<> usage_dist(40, 20);

    for (unsigned int i = 0; i < TEST_JOBCOUNT; ++i) {
      unsigned int release = release_dist(rng);

      int slack = (int)slack_dist(rng);
      while (slack < 0) {
	slack = (int)slack_dist(rng);
      }

      int duration = (int)duration_dist(rng);
      while (duration < 1) {
	duration = (int)duration_dist(rng);
      }

      double usage = usage_dist(rng);
      while (usage <= 0) {
	usage = usage_dist(rng);
      }

      Job j(release, release + (unsigned int)duration + (unsigned int)slack,
            (unsigned int)duration, {usage}, i);
      this->instance.add_job(std::move(j));
    }
  }

  Instance instance;
  std::mt19937 rng;
};

TEST_F(TreeSkyLineTest, TestConstruction) { ds::TreeSkyLine sl(&instance); }

TEST(PolymorphismSkyLineTest, TestEventEquality)
{
  Instance ins;
  Resource res(0);
  res.set_investment_costs({{1.0, 1.0}});
  ins.add_resource(std::move(res));

  Job j0(0, 100, 10, {1.0}, 0);
  Job j1(0, 100, 10, {2.0}, 1);

  ins.add_job(std::move(j0));
  ins.add_job(std::move(j1));

  ds::SkyLine sl{ds::SingleTreeSkyLine(&ins)};
  sl.insert_job(j0, 0);
  sl.insert_job(j1, 0);

  auto it1 = sl.begin();
  it1++;

  auto it2 = sl.begin();

  ASSERT_FALSE(*it1 == *it2);
}

TEST(PolymorphismSkyLineTest, TestConvertAll)
{
  Instance ins;
  Resource res(0);
  res.set_investment_costs({{1.0, 1.0}});
  ins.add_resource(std::move(res));

  Job j0(0, 100, 10, {1.0}, 0);
  Job j1(0, 100, 10, {2.0}, 1);

  ins.add_job(std::move(j0));
  ins.add_job(std::move(j1));

  ds::SkyLine sl{ds::TreeSkyLine(&ins)};
  ds::SkyLine sl2{ds::RangedTreeSkyLine(&ins)};
  ds::SkyLine sl3{ds::SingleTreeSkyLine(&ins)};
  ds::SkyLine sl4{ds::SingleRangedTreeSkyLine(&ins)};

  ds::SkyLine sl5{ds::ArraySkyLine(&ins)};
  ds::SkyLine sl6{ds::IteratorArraySkyLine(&ins)};
}

TEST(BasicSingleTreeSkyLineTest, TestGetMaximum)
{
  Instance ins;
  Resource res(0);
  res.set_investment_costs({{1.0, 1.0}});
  ins.add_resource(std::move(res));

  Job j0(0, 100, 10, {1.0}, 0);
  Job j1(0, 100, 10, {2.0}, 1);

  ins.add_job(std::move(j0));
  ins.add_job(std::move(j1));

  ds::SkyLine sl{ds::SingleTreeSkyLine{&ins}};

  sl.insert_job(j0, 0);
  sl.insert_job(j1, 0);

  auto maximum = sl.get_maximum();
  ASSERT_EQ(maximum, 3.0);
}

TEST_F(TreeSkyLineTest, TestIteration)
{
  ds::TreeSkyLine sl(&instance);

  std::vector<std::pair<unsigned int, bool>> events;
  for (auto & job : instance.get_jobs()) {
    sl.insert_job(job, job.get_release());
    events.push_back({job.get_release(), true});
    events.push_back({job.get_release() + job.get_duration(), false});
  }

  std::sort(events.begin(), events.end(), [](auto & lhs, auto & rhs) {
    if (lhs.first != rhs.first) {
      return lhs.first < rhs.first;
    }

    if (!lhs.second && rhs.second) {
      return true;
    }

    return false;
  });

  auto event_it = events.begin();
  auto sl_it = sl.begin();
  while (event_it != events.end()) {
    ASSERT_EQ(sl_it->start, event_it->second);

    if (event_it->second) {
      // Start event
      ASSERT_EQ(event_it->first, sl_it->where);
      ASSERT_EQ(sl_it->start, true);
    } else {
      ASSERT_EQ(sl_it->start, false);
      ASSERT_EQ(event_it->first, sl_it->where);
    }

    ++event_it;
    ++sl_it;
  }
}

TEST_F(TreeSkyLineTest, TestRangedConstruction)
{
  ds::RangedTreeSkyLine sl(&instance);
}

/*
 * Array based SkyLine
 */

class ArraySkyLineTest : public Test {
public:
  virtual void
  SetUp()
  {
    rng = std::mt19937(TEST_SEED);
    Resource res(0);
    res.set_investment_costs({{1.0, 1.0}});

    this->instance.add_resource(std::move(res));

    std::uniform_int_distribution<unsigned int> release_dist(
        0, 10 * TEST_JOBCOUNT);
    std::normal_distribution<> slack_dist(50, 20);
    std::normal_distribution<> duration_dist(30, 20);
    std::normal_distribution<> usage_dist(40, 20);

    for (unsigned int i = 0; i < TEST_JOBCOUNT; ++i) {
      unsigned int release = release_dist(rng);

      int slack = (int)slack_dist(rng);
      while (slack < 0) {
	slack = (int)slack_dist(rng);
      }

      int duration = (int)duration_dist(rng);
      while (duration < 1) {
	duration = (int)duration_dist(rng);
      }

      double usage = usage_dist(rng);
      while (usage <= 0) {
	usage = usage_dist(rng);
      }

      Job j(release, release + (unsigned int)duration + (unsigned int)slack,
            (unsigned int)duration, {usage}, i);
      this->instance.add_job(std::move(j));
    }
  }

  Instance instance;
  std::mt19937 rng;
};

TEST_F(ArraySkyLineTest, TestConstruction) { ds::ArraySkyLine sl(&instance); }

TEST(BasicArraySkyLineTest, TestTrivialIteration)
{
  Instance ins;
  Resource res(0);
  res.set_investment_costs({{1.0, 1.0}});
  ins.add_resource(std::move(res));

  Job j0(0, 100, 10, {1.0}, 0);
  Job j1(0, 100, 10, {2.0}, 1);

  ins.add_job(std::move(j0));
  ins.add_job(std::move(j1));

  ds::IteratorArraySkyLine sl(&ins);

  sl.insert_job(0, 0);
  sl.insert_job(1, 10);

  auto sl_it = sl.begin();
  ASSERT_TRUE(sl_it->start);
  ASSERT_EQ(sl_it->where, 0);
  sl_it++;
  ASSERT_EQ(sl_it->start, false);
  ASSERT_EQ(sl_it->where, 10);
  sl_it++;
  ASSERT_EQ(sl_it->start, true);
  ASSERT_EQ(sl_it->where, 10);
  sl_it++;
  ASSERT_EQ(sl_it->start, false);
  ASSERT_EQ(sl_it->where, 20);
  sl_it++;
  ASSERT_TRUE(sl_it == sl.end());
}

TEST_F(ArraySkyLineTest, TestIteration)
{
  ds::IteratorArraySkyLine sl(&instance);

  std::vector<std::pair<unsigned int, bool>> events;
  for (auto & job : instance.get_jobs()) {
    sl.insert_job(job, job.get_release());
    events.push_back({job.get_release(), true});
    events.push_back({job.get_release() + job.get_duration(), false});
  }

  std::sort(events.begin(), events.end(), [](auto & lhs, auto & rhs) {
    if (lhs.first != rhs.first) {
      return lhs.first < rhs.first;
    }

    if (!lhs.second && rhs.second) {
      return true;
    }

    return false;
  });

  auto event_it = events.begin();
  auto sl_it = sl.begin();
  while (event_it != events.end()) {
    ASSERT_EQ(sl_it->start, event_it->second);

    if (event_it->second) {
      // Start event
      ASSERT_EQ(event_it->first, sl_it->where);
      ASSERT_EQ(sl_it->start, true);
    } else {
      ASSERT_EQ(sl_it->start, false);
      ASSERT_EQ(event_it->first, sl_it->where);
    }

    ++event_it;
    ++sl_it;
  }
}

} // namespace skyline
} // namespace test

#endif // TCPSPSUITE_TEST_SKYLINE_HPP
