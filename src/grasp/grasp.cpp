#include "grasp.hpp"

#include <algorithm>
#include <numeric>
#include <queue>

namespace grasp {

detail::GraspRandom::GraspRandom(const Instance & instance,
                                 const SolverConfig & sconf)
    : random(sconf.was_seed_set() ? (unsigned long)sconf.get_seed() : 42ul)
{
	for (const Job & job : instance.get_jobs()) {
		jobs.push_back(&job);
	}
}

std::vector<const Job *>
detail::GraspRandom::operator()()
{
	std::shuffle(jobs.begin(), jobs.end(), random);
	return std::vector<const Job *>{jobs.begin(), jobs.end()};
}

std::string
detail::GraspRandom::getName()
{
	return "random";
}

detail::GraspSorted::GraspSorted(const Instance & instance,
                                 const SolverConfig & sconf)
{
	(void)sconf;
	for (const Job & job : instance.get_jobs()) {
		jobs.push_back(&job);
	}
	std::stable_sort(jobs.begin(), jobs.end(), [](const Job * a, const Job * b) {
		return a->get_duration() > b->get_duration();
	});
}

std::vector<const Job *>
detail::GraspSorted::operator()()
{
	return std::vector<const Job *>{jobs.begin(), jobs.end()};
}

std::string
detail::GraspSorted::getName()
{
	return "sorted";
}

implementation::GraspArray::GraspArray(const Instance & in,
                                       const SolverConfig & sconf,
                                       const Timer & timer_in)
    : instance(in), timer(timer_in), graspSelection(sconf["graspSelection"]),
      graspSamples(sconf["graspSamples"]),
      random(sconf.was_seed_set() ? (unsigned long)sconf.get_seed() : 42ul)
{
	unsigned int end = 0;
	for (const Job & job : instance.get_jobs()) {
		end = std::max(end, job.get_deadline() + 1);
	}
	usage = std::vector<ResVec>(end, ResVec(instance.resource_count()));
	timelimit = sconf.get_time_limit();
}

void
implementation::GraspArray::operator()(std::vector<const Job *> & jobs,
                                       std::vector<unsigned int> & starts)
{
	updateUsage(starts);
	auto uniform = [&](unsigned int min, unsigned int max) {
		return std::uniform_int_distribution<unsigned int>{min, max}(random);
	};
	while ((!jobs.empty()) && (timer.get() < this->timelimit)) {
		// peak, jobno, time
		std::vector<std::tuple<double, size_t, unsigned int>> candidates;

		for (unsigned int jobno = 0;
		     (jobno < std::min(this->graspSamples, (unsigned int)jobs.size())) &&
		     (timer.get() < this->timelimit);
		     ++jobno) {
			const Job * job = jobs[jobno];

			unsigned int release = job->get_release();
			unsigned int deadline = job->get_deadline();
			if (!instance.get_traits().has_flag(Traits::NO_LAGS)) {
				const LagGraph & laggraph = instance.get_laggraph();
				for (const auto & edge : laggraph.reverse_neighbors(job->get_jid())) {
					release = std::max(release, starts[edge.t] + (unsigned int)edge.lag);
				}
				for (const auto & edge : laggraph.neighbors(job->get_jid())) {
					deadline =
					    std::min(deadline, starts[edge.t] - (unsigned int)edge.lag +
					                           job->get_duration());
				}
			}

			// decrease everything in job interval with jobs resource usage
			for (unsigned int t = 0; t < job->get_duration(); ++t) {
				for (unsigned int rid = 0; rid < instance.resource_count(); rid++) {
					usage[starts[job->get_jid()] + t][rid] -=
					    job->get_resource_usage(rid);
				}
			}

			// test all valid start positions
			std::multiset<double> currentCosts;
			std::queue<std::multiset<double>::iterator> insertedCosts;

			for (unsigned int t = release; t < release + job->get_duration(); ++t) {
				insertedCosts.push(
				    currentCosts.insert(instance.calculate_costs(usage[t])));
			}

			candidates.emplace_back(*currentCosts.rbegin(), jobno, release);

			for (unsigned int t = release + 1; t <= deadline - job->get_duration();
			     ++t) {
				currentCosts.erase(insertedCosts.front());
				insertedCosts.pop();
				insertedCosts.push(currentCosts.insert(
				    instance.calculate_costs(usage[t + job->get_duration() - 1])));
				assert(jobno < jobs.size());
				candidates.emplace_back(*currentCosts.rbegin(), jobno, t);
			}

			// revert usage change
			for (unsigned int t = 0; t < job->get_duration(); ++t) {
				for (unsigned int rid = 0; rid < instance.resource_count(); rid++) {
					usage[starts[job->get_jid()] + t][rid] +=
					    job->get_resource_usage(rid);
				}
			}
		}

		std::sort(candidates.begin(), candidates.end());

		if (candidates.empty()) {
			continue;
		}

		unsigned int selected =
		    uniform(1, std::min(graspSelection, (unsigned int)candidates.size())) -
		    1;

		const Job * selectedJob = jobs[std::get<1>(candidates[selected])];
		// decrease everything in old job interval with jobs resource usage
		for (unsigned int t = 0; t < selectedJob->get_duration(); ++t) {
			for (unsigned int rid = 0; rid < instance.resource_count(); rid++) {
				usage[starts[selectedJob->get_jid()] + t][rid] -=
				    selectedJob->get_resource_usage(rid);
			}
		}

		starts[selectedJob->get_jid()] = std::get<2>(candidates[selected]);

		// increase everything in job interval with jobs resource usage
		// numerically instable?
		for (unsigned int t = 0; t < selectedJob->get_duration(); ++t) {
			for (unsigned int rid = 0; rid < instance.resource_count(); rid++) {
				usage[starts[selectedJob->get_jid()] + t][rid] +=
				    selectedJob->get_resource_usage(rid);
			}
		}

		// Remove job from job list
		jobs.erase(jobs.begin() +
		           static_cast<long>(std::get<1>(candidates[selected])));
	}
}

void
implementation::GraspArray::updateUsage(std::vector<unsigned int> & s)
{
	for (auto & u : usage) {
		u.assign(u.size(), 0);
	}
	for (const Job & job : instance.get_jobs()) {
		for (unsigned int rid = 0; rid < instance.resource_count(); rid++) {
			for (unsigned int i = 0; i < job.get_duration(); i++) {
				usage[s[job.get_jid()] + i][rid] += job.get_resource_usage(rid);
			}
		}
	}
}

std::string
implementation::GraspArray::getName()
{
	return "array";
}

implementation::GraspSkyline::GraspSkyline(const Instance & in,
                                           const SolverConfig & sconf,
                                           const Timer & timer_in)
    : instance(in), timer(timer_in), graspSelection(sconf["graspSelection"]),
      graspSamples(sconf["graspSamples"]),
      random(sconf.was_seed_set() ? (unsigned long)sconf.get_seed() : 42ul),
      usage((in.resource_count() > 1) ? ds::SkyLine{ds::TreeSkyLine{&in}}
                                      : ds::SkyLine{ds::SingleTreeSkyLine{&in}})
{
	for (const Job & job : instance.get_jobs()) {
		usage.insert_job(job, 0);
	}
	timelimit = sconf.get_time_limit();
}

void
implementation::GraspSkyline::operator()(std::vector<const Job *> & jobs,
                                         std::vector<unsigned int> & starts)
{
	updateUsage(starts);
	auto uniform = [&](unsigned int min, unsigned int max) {
		return std::uniform_int_distribution<unsigned int>{min, max}(random);
	};
	auto getPos = [](const auto & e) { return e.where; };

	while ((!jobs.empty()) && (timer.get() < this->timelimit)) {
		//(cost, jobno, pos, length)
		std::vector<std::tuple<Resources, size_t, unsigned int, int>> candidates;
		size_t length_sum = 0;

		for (unsigned int jobno = 0;
		     (jobno < std::min(graspSamples, (unsigned int)jobs.size()) &&
		      (timer.get() < this->timelimit));
		     ++jobno) {
			const Job * job = jobs[jobno];

			usage.remove_job(*job);
			unsigned int release = job->get_release();
			unsigned int deadline = job->get_deadline();
			if (!instance.get_traits().has_flag(Traits::NO_LAGS)) {
				const LagGraph & laggraph = instance.get_laggraph();
				for (const auto & edge : laggraph.reverse_neighbors(job->get_jid())) {
					release = std::max(release, starts[edge.t] + (unsigned int)edge.lag);
				}
				for (const auto & edge : laggraph.neighbors(job->get_jid())) {
					deadline =
					    std::min(deadline, starts[edge.t] - (unsigned int)edge.lag +
					                           job->get_duration());
				}
			}

			std::vector<unsigned int> startPos = {release};
			auto it = usage.upper_bound(release);
			while (it != usage.end() && getPos(*it) <= deadline) {
				if (getPos(*it) <= deadline - job->get_duration()) {
					startPos.push_back(getPos(*it));
				}
				if (getPos(*it) >= release + job->get_duration()) {
					startPos.push_back(getPos(*it) - job->get_duration());
				}
				it++;
			}
			startPos.push_back(deadline - job->get_duration() + 1);
			std::sort(startPos.begin(), startPos.end());
			startPos.erase(std::unique(startPos.begin(), startPos.end()),
			               startPos.end());

			for (unsigned int i = 0; i + 1 < startPos.size(); i++) {
				Resources cost =
				    usage.get_maximum(startPos[i], startPos[i] + job->get_duration());
				unsigned int s = startPos[i];
				int l =
				    static_cast<int>(startPos[i + 1]) - static_cast<int>(startPos[i]);
				candidates.push_back({cost, jobno, s, l});
				length_sum =
				    static_cast<unsigned int>(static_cast<int>(length_sum) + l);
			}

			// Revert the changes we made to the usages
			usage.insert_job(*job, starts[job->get_jid()]);
		}

		if (candidates.empty()) {
			continue;
		}

		std::sort(candidates.begin(), candidates.end());
		int selected = (int)uniform(
		    0, std::min(graspSelection - 1, (unsigned int)length_sum - 1));

		for (unsigned int i = 0; (selected >= 0) && (i < candidates.size()); i++) {
			if (selected < std::get<3>(candidates[i])) {
				const Job * selectedJob = jobs[std::get<1>(candidates[i])];

				starts[selectedJob->get_jid()] =
				    std::get<2>(candidates[i]) + static_cast<unsigned int>(selected);
				jobs.erase(jobs.begin() +
				           static_cast<long>(std::get<1>(candidates[i])));
				usage.set_pos(*selectedJob, starts[selectedJob->get_jid()]);
				break;
			}
			selected -= std::get<2>(candidates[i]);
		}
	}
}

std::string
implementation::GraspSkyline::getName()
{
	return "skyline";
}

void
implementation::GraspSkyline::updateUsage(std::vector<unsigned int> & s)
{
	for (const Job & job : instance.get_jobs()) {
		usage.set_pos(job, s[job.get_jid()]);
	}
}

template <typename GraspAlgorithm, typename GraspImplementation>
GRASP<GraspAlgorithm, GraspImplementation>::GRASP(
    const Instance & instance_in, AdditionalResultStorage & additional,
    const SolverConfig & sconf)
    : instance(instance_in), storage(additional), bestCosts(1.0 / 0.0),
      bestStarts(instance_in.job_count()), starts(instance_in.job_count()),
      l("GRASP"), graspAlgorithm(instance_in, sconf),
      graspImplementation(instance_in, sconf, timer),
      weightedSelections(sconf["weightedSelections"]),
      weightedIterations(sconf["weightedIterations"]),
      uniformSelections(sconf["uniformSelections"]),
      uniformIterations(sconf["uniformIterations"]),
      graspSelection(sconf["graspSelection"]),
      graspSamples(sconf["graspSamples"]),
      resetCount(sconf.has_config("resetCount")
                     ? (unsigned int)sconf["resetCount"]
                     : 0u),
      nextReset(resetCount),
      writeIntermediateInterval(
          ((bool)sconf.has_config("intermediate_score_interval"))
              ? sconf["intermediate_score_interval"].get<double>()
              : 0),
      lastIntermediateTime(0),
      writeTempResult(!sconf.has_config("writeTemp") || sconf["writeTemp"]),
      random(sconf.was_seed_set() ? (unsigned long)sconf.get_seed() : 42ul),
      permutation(instance_in.job_count())
{
	(void)additional;
	if (!sconf.get_time_limit().valid()) {
		BOOST_LOG(l.e()) << "GRASP needs a time limit!";
		throw "GRASP needs a time limit.";
	}
	timelimit = sconf.get_time_limit();
	std::iota(permutation.begin(), permutation.end(), 0U);
}

template <typename GraspAlgorithm, typename GraspImplementation>
void
GRASP<GraspAlgorithm, GraspImplementation>::run()
{
	CriticalPathComputer cpc(instance);
	starts = cpc.get_forward();
	this->timer.start();
	unsigned int iteration = 0;
	double graspTime = 0;
	double hillClimberTime = 0;
	while (this->timer.get() < this->timelimit) {
		iteration++;
		double start;
		start = timer.get();
		grasp();
		graspTime += timer.get() - start;
		start = timer.get();
		double costs = hillClimber();
		hillClimberTime += timer.get() - start;

		if (__builtin_expect(this->writeIntermediateInterval > 0, false)) {
			double time = timer.get();
			if ((time - this->lastIntermediateTime) >
			    this->writeIntermediateInterval) {
				storage.intermediate_results.push_back(
				    {Maybe<double>(time), Maybe<unsigned int>(iteration),
				     Maybe<double>(this->bestCosts), Maybe<double>(),
				     Maybe<Solution>()});
				this->lastIntermediateTime = time;
			}
		}

		if (writeTempResult) {
			storage.intermediate_results.push_back(
			    {Maybe<double>(timer.get()), Maybe<unsigned int>(iteration),
			     Maybe<double>(costs), Maybe<double>(), Maybe<Solution>()});
		}
		if (costs < bestCosts) {
			bestStarts = starts;
			bestCosts = costs;
			nextReset = resetCount;
		} else {
			nextReset--;
			if (resetCount != 0 && nextReset == 0) {
				starts = bestStarts;
				costs = bestCosts;
				nextReset = resetCount;
			}
		}
	}
	storage.extended_measures.push_back(
	    {"iterations", iteration, this->timelimit,
	     AdditionalResultStorage::ExtendedMeasure::TYPE_INT, (int)iteration});
	storage.extended_measures.push_back(
	    {"GraspTime", iteration, this->timelimit,
	     AdditionalResultStorage::ExtendedMeasure::TYPE_DOUBLE, graspTime});
	storage.extended_measures.push_back(
	    {"HillClimberTime", iteration, this->timelimit,
	     AdditionalResultStorage::ExtendedMeasure::TYPE_DOUBLE, hillClimberTime});
}

template <typename GraspAlgorithm, typename GraspImplementation>
Solution
GRASP<GraspAlgorithm, GraspImplementation>::get_solution()
{
	return Solution(this->instance, false, this->bestStarts,
	                this->get_lower_bound());
}

template <typename GraspAlgorithm, typename GraspImplementation>
std::string
GRASP<GraspAlgorithm, GraspImplementation>::get_id()
{
	using namespace std::literals;
	return "GRASP <"s + GraspAlgorithm::getName() + ", "s +
	       GraspImplementation::getName() + ">"s;
}

template <typename GraspAlgorithm, typename GraspImplementation>
Maybe<double>
GRASP<GraspAlgorithm, GraspImplementation>::get_lower_bound()
{
	return Maybe<double>();
}

template <typename GraspAlgorithm, typename GraspImplementation>
const Traits &
GRASP<GraspAlgorithm, GraspImplementation>::get_requirements()
{
	return required_traits;
}

template <typename GraspAlgorithm, typename GraspImplementation>
void
GRASP<GraspAlgorithm, GraspImplementation>::grasp()
{
	if (graspSelection == 0) {
		return;
	} else {
		std::vector<const Job *> jobs = graspAlgorithm();
		graspImplementation(jobs, starts);
	}
}

template <typename GraspAlgorithm, typename GraspImplementation>
double
GRASP<GraspAlgorithm, GraspImplementation>::hillClimber()
{
	auto uniform = [&](unsigned int min, unsigned int max) {
		return std::uniform_int_distribution<unsigned int>{min, max}(random);
	};
	auto uniformReal = [&](double min, double max) {
		return std::uniform_real_distribution<double>{min, max}(random);
	};
	double cost = instance.calculate_max_costs(starts);
	// uniform selection:
	for (unsigned int i = 0;
	     (i < uniformIterations) && (this->timer.get() < this->timelimit); i++) {
		std::vector<unsigned int> newStarts = starts;
		for (unsigned int j = 0;
		     (j < std::min(uniformSelections, instance.job_count())); j++) {
			std::swap(permutation[j],
			          permutation[uniform(j, instance.job_count() - 1)]);
			const Job & job = instance.get_job(permutation[j]);
			unsigned int release = job.get_release();
			unsigned int deadline = job.get_deadline();
			if (!instance.get_traits().has_flag(Traits::NO_LAGS)) {
				const LagGraph & laggraph = instance.get_laggraph();
				for (const auto & edge : laggraph.reverse_neighbors(job.get_jid())) {
					release =
					    std::max(release, newStarts[edge.t] + (unsigned int)edge.lag);
				}
				for (const auto & edge : laggraph.neighbors(job.get_jid())) {
					deadline =
					    std::min(deadline, newStarts[edge.t] - (unsigned int)edge.lag +
					                           job.get_duration());
				}
			}
			newStarts[permutation[j]] =
			    uniform(release, deadline - job.get_duration());
		}
		double newCost = instance.calculate_max_costs(newStarts);
		if (newCost < cost) {
			cost = newCost;
			starts = newStarts;
		}
	}

	// weighted selection:
	std::vector<ResVec> usage = resourceUsage(starts);
	for (unsigned int i = 0;
	     (i < weightedIterations) && (this->timer.get() < this->timelimit); i++) {
		std::vector<unsigned int> newStarts = starts;
		std::vector<ResVec> newUsage = usage;
		double newCost = cost;
		for (unsigned int j = 0;
		     j < std::min(weightedSelections, instance.job_count()); j++) {
			std::swap(permutation[j],
			          permutation[uniform(j, instance.job_count() - 1)]);
			const Job & job = instance.get_job(permutation[j]);

			unsigned int release = job.get_release();
			unsigned int deadline = job.get_deadline();
			if (!instance.get_traits().has_flag(Traits::NO_LAGS)) {
				const LagGraph & laggraph = instance.get_laggraph();
				for (const auto & edge : laggraph.reverse_neighbors(job.get_jid())) {
					release =
					    std::max(release, newStarts[edge.t] + (unsigned int)edge.lag);
				}
				for (const auto & edge : laggraph.neighbors(job.get_jid())) {
					deadline =
					    std::min(deadline, newStarts[edge.t] - (unsigned int)edge.lag +
					                           job.get_duration());
				}
			}

			// increase everything in job interval with jobs resource usage
			for (unsigned int t = release; t < newStarts[job.get_jid()]; ++t) {
				for (unsigned int rid = 0; rid < instance.resource_count(); rid++) {
					newUsage[t][rid] += job.get_resource_usage(rid);
				}
			}
			for (unsigned int t = newStarts[job.get_jid()] + job.get_duration();
			     t < deadline; ++t) {
				for (unsigned int rid = 0; rid < instance.resource_count(); rid++) {
					newUsage[t][rid] += job.get_resource_usage(rid);
				}
			}
			// test all valid start positions
			std::multiset<double> currentCosts;
			std::queue<std::multiset<double>::iterator> insertedCosts;
			std::vector<double> newCosts(deadline - job.get_duration() + 1 - release);
			std::vector<unsigned int> betterStarts;
			double betterStartsSum = 0;
			double reciprocalSum = 0;
			for (unsigned int t = release; t < release + job.get_duration(); ++t) {
				insertedCosts.push(
				    currentCosts.insert(instance.calculate_costs(newUsage[t])));
			}
			newCosts[0] = *currentCosts.rbegin();
			reciprocalSum += 1.0 / newCosts[0];
			if (newCosts[0] < cost) {
				betterStarts.push_back(release);
				betterStartsSum -= newCosts[0] - newCost;
			}
			for (unsigned int t = release + 1; t <= deadline - job.get_duration();
			     ++t) {
				currentCosts.erase(insertedCosts.front());
				insertedCosts.pop();
				insertedCosts.push(currentCosts.insert(
				    instance.calculate_costs(newUsage[t + job.get_duration() - 1])));
				newCosts[t - release] = *currentCosts.rbegin();
				reciprocalSum += 1.0 / newCosts[t - release];
				if (newCosts[t - release] < newCost) {
					betterStarts.push_back(t);
					betterStartsSum -= newCosts[t - release] - newCost;
				}
			}

			if (betterStarts.empty()) {
				double selection = uniformReal(0, reciprocalSum);
				// TODO binary search?
				double sum = 0;
				unsigned int index = release;
				while (index < deadline - job.get_duration() &&
				       selection >= sum + 1.0 / newCosts[index - release]) {
					sum += 1.0 / newCosts[index - release];
					index++;
				}
				newStarts[job.get_jid()] = index;
				newCost = newCosts[index - release];
			} else {
				double selection = uniformReal(0, betterStartsSum);
				// TODO binary search?
				double sum = 0;
				unsigned int index = 0;
				while (index + 1 < betterStarts.size() &&
				       selection > sum + betterStarts[index]) {
					sum += betterStarts[index];
					index++;
				}
				newStarts[job.get_jid()] = betterStarts[index];
				newCost = newCosts[newStarts[job.get_jid()] - release];
			}

			// decrease resource usage everywhere besides new start
			// numerically instable?
			for (unsigned int t = release; t < newStarts[job.get_jid()]; ++t) {
				for (unsigned int rid = 0; rid < instance.resource_count(); rid++) {
					newUsage[t][rid] -= job.get_resource_usage(rid);
				}
			}
			for (unsigned int t = newStarts[job.get_jid()] + job.get_duration();
			     t < deadline; ++t) {
				for (unsigned int rid = 0; rid < instance.resource_count(); rid++) {
					newUsage[t][rid] -= job.get_resource_usage(rid);
				}
			}
		}
		if (newCost < cost) {
			cost = newCost;
			starts = newStarts;
		}
	}
	return cost;
}

template <typename GraspAlgorithm, typename GraspImplementation>
std::vector<ResVec>
GRASP<GraspAlgorithm, GraspImplementation>::resourceUsage(
    std::vector<unsigned int> & s)
{
	unsigned int end = 0;
	for (const Job & job : instance.get_jobs()) {
		end = std::max(end, job.get_deadline() + 1);
	}
	std::vector<ResVec> usage(end, ResVec(instance.resource_count()));
	for (const Job & job : instance.get_jobs()) {
		for (unsigned int rid = 0; rid < instance.resource_count(); rid++) {
			for (unsigned int i = 0; i < job.get_duration(); i++) {
				usage[s[job.get_jid()] + i][rid] += job.get_resource_usage(rid);
			}
		}
	}
	return usage;
}

template class GRASP<detail::GraspRandom, implementation::GraspArray>;
template class GRASP<detail::GraspRandom, implementation::GraspSkyline>;
template class GRASP<detail::GraspSorted, implementation::GraspArray>;
template class GRASP<detail::GraspSorted, implementation::GraspSkyline>;

} // namespace grasp
