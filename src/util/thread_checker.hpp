#ifndef THREAD_CHECKER_HPP
#define THREAD_CHECKER_HPP

#include "log.hpp"
#include "../manager/timer.hpp"
#include <mutex>                       // for mutex

namespace util {

class ThreadChecker {
public:
	static ThreadChecker & get();
	void check(int thread_id);
private:
	Timer t;
	std::mutex m;
	std::map<int, double> last_checked;
	ThreadChecker();

	Log l;
};

}

#endif
