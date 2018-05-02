#ifndef INTERVALRUNNER_HPP
#define INTERVALRUNNER_HPP

#include "../datastructures/maybe.hpp"

template<class It1, class It2, class Point, class PointGetter, class EventHandler, class DataGetter, class Data>
void process_intervals(It1 start_a, It1 end_a, It2 start_b, It2 end_b, PointGetter &pg, EventHandler &handler, DataGetter &dg) {
  using CbData = Maybe<Data>;

  Point last_point = Point();

  if ((start_a != end_a) && ((start_b == end_b) || (pg(start_a, true) < pg(start_b, true)))) {
    last_point = pg(start_a, true);
  }

  bool event_a;
  bool event_b;

  if ((start_a != end_a) && (pg(start_a, true) == last_point)) {
    event_a = true;
  } else {
    event_a = false;
  }

  if ((start_b != end_b) && (pg(start_b, true) == last_point)) {
    event_b = true;
  } else {
    event_b = false;
  }

  handler(last_point,
          event_a ? CbData(dg(start_a)) : CbData(),
          CbData(),
          event_b ? CbData(dg(start_b)) : CbData(),
          CbData()
        );

  while ((start_a != end_a) || (start_b != end_b)) {
    Point next_point = last_point;

    bool a_ends = false;
    bool a_starts = false;
    bool b_ends = false;
    bool b_starts = false;

    if (start_a != end_a) {
      if (pg(start_a, true) > last_point) {
        a_starts = true;
        next_point = pg(start_a, true);
      } else {
#ifdef ENABLE_ASSERTIONS
        assert(pg(start_a, false) > last_point);
#endif
        a_ends = true;
        next_point = pg(start_a, false);
        if ((start_a + 1) != end_a) {
          if (pg((start_a + 1), true) == next_point) {
            a_starts = true;
          }
        }
      }
    }

    if (start_b != end_b) {
      //std::cout << "Comparing for B: " << pg(start_b, true) << " vs " << last_point << "\n";
      if (pg(start_b, true) > last_point) {
        //std::cout << "Taking B's lower point\n";

        if ((next_point == last_point) || (pg(start_b, true) < next_point)) {
          b_starts = true;
          a_starts = false;
          a_ends = false;
          next_point = pg(start_b, true);
        } else if (pg(start_b, true) == next_point) {
          b_starts = true;
        }
      } else {
#ifdef ENABLE_ASSERTIONS
        assert(pg(start_b, false) > last_point);
#endif
        if ((next_point == last_point) || (pg(start_b, false) < next_point)) {
          b_starts = false;
          b_ends = true;
          a_starts = false;
          a_ends = false;
          next_point = pg(start_b, false);
        } else if (pg(start_b, false) == next_point) {
          b_ends = true;
        }

        if ((b_ends) && ((start_b + 1) != end_b) && (pg((start_b + 1), true) == next_point)) {
          b_starts = true;
        }
      }
    }
    //std::cout << "A Starts: " << a_starts << " / A Ends: " << a_ends ;
    //std::cout << "B Starts: " << b_starts << " / B Ends: " << b_ends ;
    //std::cout << "Next point: " << next_point ;

    handler(next_point,
      // start data in A: start_a starts here
      a_starts && (!a_ends) ? CbData(dg(start_a)) :
        // Start data in A: (start_a + 1) starts here
        a_starts && a_ends ? CbData(dg(start_a + 1)) :
          // Start data in A: nothing starts here
          CbData(),

      a_ends ? CbData(dg(start_a)) : CbData(),

      // start data in B: start_b starts here
      b_starts && (!b_ends) ? CbData(dg(start_b)) :
        // Start data in B: (start_b + 1) starts here
        b_starts && b_ends ? CbData(dg(start_b + 1)) :
          // Start data in B: nothing starts here
          CbData(),

      b_ends ? CbData(dg(start_b)) : CbData()
    );

    last_point = next_point;
    if (a_ends) {
      start_a++;
    }
    if (b_ends) {
      start_b++;
    }
  }
}

#endif
