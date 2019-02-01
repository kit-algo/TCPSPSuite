#include "thread_checker.hpp"
#include "configuration.hpp"

namespace util {

ThreadChecker::ThreadChecker() : l("TCHK") { this->t.start(); }

ThreadChecker &
ThreadChecker::get()
{
	static ThreadChecker instance;
	return instance;
}

void
ThreadChecker::check(int thread_id)
{
	auto cfg = Configuration::get();
	if (!cfg->get_thread_check_time().valid()) {
		return;
	}

	double check_time = cfg->get_thread_check_time();

	std::lock_guard(this->m);

	BOOST_LOG(l.i()) << "Checking thread health...";

	double time = this->t.get();
	this->last_checked[thread_id] = time;

	size_t alive = 0;
	size_t stuck = 0;
	for (auto [last_id, last_time] : this->last_checked) {
		if ((time - last_time) > check_time) {
			// UH-OH.
			BOOST_LOG(l.e()) << "Thread " << (last_id+1)
			                 << " seems to be stuck. Last check-in was "
			                 << (time - last_time) << " seconds ago.";
			stuck++;
		} else {
			alive++;
		}
	}
	BOOST_LOG(l.i()) << alive << " threads alive, " << stuck << " stuck.";
}

}
