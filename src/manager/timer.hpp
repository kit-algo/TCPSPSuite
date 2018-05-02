#ifndef TIMER_H
#define TIMER_H

#include <chrono>

class Timer {
public:
  void start();
  double stop();
  double get() const;

private:
  std::chrono::steady_clock::time_point started;
};

#endif
