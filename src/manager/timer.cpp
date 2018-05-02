#include "timer.hpp"
#include <ratio>  // for ratio

void
Timer::start()
{
  this->started = std::chrono::steady_clock::now();
}

// TODO should this actually 'stop' somehow?
double
Timer::stop()
{
  std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

  std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(now - this->started);
  return time_span.count();
}

double
Timer::get() const
{
  std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

  std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(now - this->started);
  return time_span.count();
}
