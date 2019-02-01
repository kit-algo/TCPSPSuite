#ifndef GRASP_HPP
#define GRASP_HPP

#include "instance/instance.hpp"
#include "instance/solution.hpp"
#include "manager/timer.hpp"
#include "datastructures/maybe.hpp"
#include "db/storage.hpp"
#include "util/solverconfig.hpp"
#include "util/log.hpp"
#include "../manager/solvers.hpp"
#include "../datastructures/skyline.hpp"
#include "../instance/resource.hpp"
#include "../algorithms/graphalgos.hpp"

#include <queue>
#include <random>

#include <typeinfo>

namespace grasp {
  namespace detail {
    class GraspRandom {
    public:
      GraspRandom(const Instance& instance, const SolverConfig& sconf);
      
      std::vector<const Job*> operator()();

      static std::string getName();
      
    private:
      std::mt19937 random;
      std::vector<const Job*> jobs;
    };    
        
    class GraspSorted {
    public:
      GraspSorted(const Instance& instance, const SolverConfig& sconf);
      
      std::vector<const Job*> operator()();

      static std::string getName();
      
    private:
      std::vector<const Job*> jobs;
    };
  }

  namespace implementation {
    class GraspArray {
    public:
	    GraspArray(const Instance& in, const SolverConfig& sconf, const Timer & timer_in);

      void operator()(std::vector<const Job*>& jobs, std::vector<unsigned int>& starts);

      static std::string getName();

    private:
      const Instance& instance;
	    const Timer & timer;
	    double timelimit;
	    
      const unsigned int graspSelection;
	    const unsigned int graspSamples;
	    
      std::mt19937 random;
      std::vector<ResVec> usage;

      void updateUsage(std::vector<unsigned int>& s);
    };

    class GraspSkyline {
    public:
	    GraspSkyline(const Instance& in, const SolverConfig& sconf, const Timer & timer_in);

      void operator()(std::vector<const Job*>& jobs, std::vector<unsigned int>& starts);

      static std::string getName();

    private:
      const Instance& instance;
	    const Timer & timer;
	    double timelimit;
	    
      const unsigned int graspSelection;
	    const unsigned int graspSamples;
	    
      std::mt19937 random;
      ds::SkyLine usage;

      void updateUsage(std::vector<unsigned int>& s);
    };
  }

  template<typename GraspAlgorithm = detail::GraspRandom, typename GraspImplementation = implementation::GraspArray>
  class GRASP {
  private:
    const Instance &instance;
    AdditionalResultStorage& storage;
    double bestCosts;
    std::vector<unsigned int> bestStarts;
    std::vector<unsigned int> starts;
    Timer timer;
    double timelimit;
    Log l;

    static const Traits required_traits;

    GraspAlgorithm graspAlgorithm;
    GraspImplementation graspImplementation;

    const unsigned int weightedSelections;
    const unsigned int weightedIterations;
    const unsigned int uniformSelections;
    const unsigned int uniformIterations;

    const unsigned int graspSelection;
	  const unsigned int graspSamples;
	  
    const unsigned int resetCount;
    unsigned int nextReset;

	  /* Writing intermediate solutions */
	  const double writeIntermediateInterval;
	  double lastIntermediateTime;
    const bool writeTempResult;
    
    std::mt19937 random;
    std::vector<unsigned int> permutation;

  public:
    /**
     * Constructs a new solver
     *
     * weightedSelections  number of Hill climber steps
     * weightedIterations  number of jobs moved in one hill climber step
     * uniformSelections   number of Hill climber steps
     * uniformIterations   number of jobs moved in one hill climber step
     * graspSelection      number of best positions to randomly select from for each job
     * writeTemp           log intermediate results
     * resetCount          number of iterations without improvement before reset
     *
     * @param instance_in  The TCPSP instance that should be solved
     * @param additional   storage for additional data (unused)
     * @param sconf        The configuration of this Solver
     */
    GRASP(const Instance& instance_in, AdditionalResultStorage& additional, const SolverConfig& sconf);

    /**
     * Calculates the result of this solver
     */
    void run();
    
    /**
     * Returns the solution of this Solver
     *
     * @return the solution of this Solver
     */
    Solution get_solution();

    /**
     * Returns the unique name of the solver
     *
     * @return the unique name of the solver
     */
    static std::string get_id();

    /**
     * Returns the lower bound found by this solver
     *
     * @return the lower bound found by this solver
     */
    Maybe<double> get_lower_bound();

    /**
     * Returns the traits required by this solver
     *
     * @return the required traits
     */
    static const Traits &get_requirements();

  private:
  
    void grasp();
    double hillClimber();    
    std::vector<ResVec> resourceUsage(std::vector<unsigned int>& s);

  };

  template<typename GraspAlgorithm, typename GraspImplementation>
  const Traits GRASP<GraspAlgorithm, GraspImplementation>::required_traits = Traits(
      Traits::LAGS_ONLY_POSITIVE | Traits::LAGS_DAG | Traits::NO_WINDOW_EXTENSION | 
      Traits::NO_DRAIN | Traits::FLAT_AVAILABILITY,
      std::numeric_limits<unsigned int>::max(),
      {}, {});
}



// Register the solvers
namespace solvers {

// grasp::GRASP<grasp::detail::GraspRandom, grasp::implementation::GraspArray>
template <>
struct registry_hook<solvers::get_free_N<grasp::GRASP<grasp::detail::GraspRandom, grasp::implementation::GraspArray>>()>
{
  constexpr static unsigned int my_N = solvers::get_free_N<grasp::GRASP<grasp::detail::GraspRandom, grasp::implementation::GraspArray>>();

  auto
  operator()()
  {
    return solvers::register_class < grasp::GRASP<grasp::detail::GraspRandom, grasp::implementation::GraspArray>, my_N > {}();
  }
};

// grasp::GRASP<grasp::detail::GraspSorted, grasp::implementation::GraspArray>
template <>
struct registry_hook<solvers::get_free_N<grasp::GRASP<grasp::detail::GraspSorted, grasp::implementation::GraspArray>>()>
{
  constexpr static unsigned int my_N = solvers::get_free_N<grasp::GRASP<grasp::detail::GraspSorted, grasp::implementation::GraspArray>>();

  auto
  operator()()
  {
    return solvers::register_class < grasp::GRASP<grasp::detail::GraspSorted, grasp::implementation::GraspArray>, my_N > {}();
  }
};

// grasp::GRASP<grasp::detail::GraspRandom, grasp::implementation::GraspSkyline>
template <>
struct registry_hook<solvers::get_free_N<grasp::GRASP<grasp::detail::GraspRandom, grasp::implementation::GraspSkyline>>()>
{
  constexpr static unsigned int my_N = solvers::get_free_N<grasp::GRASP<grasp::detail::GraspRandom, grasp::implementation::GraspSkyline>>();

  auto
  operator()()
  {
    return solvers::register_class < grasp::GRASP<grasp::detail::GraspRandom, grasp::implementation::GraspSkyline>, my_N > {}();
  }
};

// grasp::GRASP<grasp::detail::GraspSorted, grasp::implementation::GraspSkyline>
template <>
struct registry_hook<solvers::get_free_N<grasp::GRASP<grasp::detail::GraspSorted, grasp::implementation::GraspSkyline>>()>
{
  constexpr static unsigned int my_N = solvers::get_free_N<grasp::GRASP<grasp::detail::GraspSorted, grasp::implementation::GraspSkyline>>();

  auto
  operator()()
  {
    return solvers::register_class < grasp::GRASP<grasp::detail::GraspSorted, grasp::implementation::GraspSkyline>, my_N > {}();
  }
};

}

#endif
