#include "visualizer.hpp"

#include <cairo-svg.h>
#include <set>
#include <iostream>

#define NDEBUG

// Equal starts and ends really mess up everything
#define HEIGHT_EPSILON 0.001

// TODO respect drain!

Visualizer::Visualizer(const Solution &solution_in)
  : solution(solution_in), instance(*solution_in.get_instance())
{
}

Visualizer::~Visualizer()
{

}

void
Visualizer::draw_field(unsigned int res_id)
{
  double base_offset = 0.0;
  for (unsigned int i = 0 ; i < res_id ; ++i) {
    base_offset += this->field_height[i];
  }

  // draw X axis
  cairo_move_to(cr, BASE_WIDTH, this->img_height - (base_offset + 2*BASE_HEIGHT));
  cairo_line_to(cr, this->img_width - BASE_WIDTH, this->img_height - (base_offset + 2*BASE_HEIGHT));

  // draw Y axis
  cairo_move_to(cr, 2*BASE_WIDTH, this->img_height - (base_offset + BASE_HEIGHT));
  cairo_line_to(cr, 2*BASE_WIDTH, this->img_height - (base_offset + this->field_height[res_id] - BASE_HEIGHT));

  // draw X ticks
  double tick_width = 0.0;
  while (tick_width < this->img_width) {
    cairo_move_to(cr, 2*BASE_WIDTH + (tick_width), this->img_height - (base_offset + 2*BASE_HEIGHT));
    cairo_line_to(cr, 2*BASE_WIDTH + (tick_width), this->img_height - (base_offset + 2*BASE_HEIGHT - TICK_SIZE));
    tick_width += TIME_UNIT_WIDTH;
  }

  // draw Y ticks
  double tick_height = 0.0;
  while (tick_height < this->field_height[res_id]) {
    cairo_move_to(cr, 2*BASE_WIDTH, this->img_height - (base_offset + 2*BASE_HEIGHT + tick_height));
    cairo_line_to(cr, 2*BASE_WIDTH - TICK_SIZE, this->img_height - (base_offset + 2*BASE_HEIGHT + tick_height));
    tick_height += RES_UNIT_HEIGHT;
  }

  for (unsigned int job_id = 0 ; job_id < this->instance.job_count() ; ++job_id) {
    if (!this->solution.job_scheduled(job_id)) {
      continue;
    }

    double left = this->solution.get_start_time(job_id) * TIME_UNIT_WIDTH + 2*BASE_WIDTH;
    double right = (this->solution.get_start_time(job_id) + this->instance.get_job(job_id).get_duration()) * TIME_UNIT_WIDTH + 2*BASE_WIDTH;

    double height = this->instance.get_job(job_id).get_resource_usage(res_id) * RES_UNIT_HEIGHT;
    double bottom = this->img_height - (base_offset + 2*BASE_HEIGHT + this->baselines[res_id][job_id]);
    double top = bottom - height;

    cairo_move_to(cr, left, bottom);
    cairo_line_to(cr, right, bottom);
    cairo_line_to(cr, right, top);
    cairo_line_to(cr, left, top);
    cairo_line_to(cr, left, bottom);
    cairo_stroke(cr);

    // Draw the number!
    std::string label = std::to_string(job_id);
    cairo_text_extents_t te;
    cairo_text_extents (cr, label.c_str(), &te);
    cairo_move_to (cr, (left + right) / 2.0 - te.x_bearing - te.width - te.y_bearing - te.height / 2, (bottom + top) / 2.0 - te.y_bearing - te.height / 2);
    //cairo_move_to(cr, left + 3, bottom - 3);
    cairo_show_text (cr, label.c_str());
    cairo_fill(cr);
  }

}

std::map<unsigned int, double>
Visualizer::arrange_heights(unsigned int res_id)
{
  std::vector<std::pair<unsigned int, unsigned int>> events;

  for (unsigned int job_id = 0 ; job_id < this->instance.job_count() ; ++job_id) {
    if (!this->solution.job_scheduled(job_id)) {
      continue;
    }
    events.push_back(std::make_pair(this->solution.get_start_time(job_id), job_id));
    events.push_back(std::make_pair(this->solution.get_start_time(job_id) + this->instance.get_job(job_id).get_duration(), job_id));
  }

  std::sort(events.begin(), events.end(), [&](std::pair<unsigned int, unsigned int> &ev1, std::pair<unsigned int, unsigned int> &ev2) {
    if (ev1.first != ev2.first) {
      return ev1.first < ev2.first;
    }

    // If both events are at the same point in time, we want to process starts first
    if (this->solution.get_start_time(ev1.second) == ev1.first) {
      // ev1 starts here!
      if (this->solution.get_start_time(ev2.second) == ev2.first) {
        // ev2 starts here, too. Force a total ordering by comparing IDs
        return ev1.second < ev2.second;
      } else {
        // ev2 ends. ev1 shoud be first!
        return false;
      }
    } else {
      // ev1 ends here
      if (this->solution.get_start_time(ev2.second) == ev2.first) {
        // ev2 starts here, ev2 should be first
        return true;
      } else {
        // both end. Order by IDs
        return ev1.second < ev2.second;
      }
    }
  });

  std::set<double> start_candidates;
  std::set<double> end_candidates;

  std::map<unsigned int, double> baseline;

  start_candidates.insert(0.0);

  for (auto ev : events) {
    double height = this->instance.get_job(ev.second).get_resource_usage(res_id) * RES_UNIT_HEIGHT;

    if (height < HEIGHT_EPSILON) {
      // This job does not use this resource.
      continue;
    }

#ifndef NDEBUG
    std::cout << "===============================\n";
    std::cout << "Job " << ev.second << " (height: " << height << ")";
    if (this->solution.get_start_time(ev.second) == ev.first) {
      std::cout << " starts ";
    } else {
      std::cout << " ends ";
    }
    std::cout << "at " << ev.first << "\n";
    std::cout << "Current start candidates: ";
    for (auto start_candidate : start_candidates) {
      std::cout << start_candidate << ", ";
    }
    std::cout << "\n";
    std::cout << "Current end candidates: ";
    for (auto end_candidate : end_candidates) {
      std::cout << end_candidate << ", ";
    }
    std::cout << "\n";

#endif

    if (this->solution.get_start_time(ev.second) == ev.first) {
      // This is a start! Determine a possible start position and update candidates

      for (auto start_candidate : start_candidates) {
        auto end_candidate_it = end_candidates.upper_bound(start_candidate);

#ifndef NDEBUG
        std::cout << "Start candidate: " << start_candidate;
        if (end_candidate_it != end_candidates.end()) {
          std::cout << " end candidate: " << *end_candidate_it << "\n";
          std::cout << "Comparing: " << *end_candidate_it << " >= " << (start_candidate + height) << "\n";
        } else {
          std::cout << " end candidate: infinity\n";
        }
#endif

        if ((end_candidate_it == end_candidates.end()) || (*end_candidate_it >= start_candidate + height)) {
          // We found a spot!
          baseline[ev.second] = start_candidate;

          // Cache values since we will now send those iterators to nirvana
          double start_point = start_candidate;
          double end_point = *end_candidate_it;

          start_candidates.erase(start_point);

          if ((end_candidate_it == end_candidates.end()) || (*end_candidate_it > start_candidate + height + 1)) { // TODO constant instead of one?
            start_candidates.insert(start_candidate + height + 1);
          } else {
            end_candidates.erase(end_point);
          }

          break; // OMGEXIT. Iterators are completely trashed.
        }
      }
    } else {
      double started_at = baseline[ev.second];
      double ended_at = started_at + height;

      // See to the top.
      if (start_candidates.find(ended_at + 1.0) == start_candidates.end()) {
        // There is something blocking above, just insert a new end
        end_candidates.insert(ended_at);
      } else {
        start_candidates.erase(ended_at + 1.0);
      }

      // See to the bottom
      if (end_candidates.find(started_at - 1.0) == end_candidates.end()) {
        // Something is blocking below, *or* we are the bottom
        start_candidates.insert(started_at);
      } else {
        end_candidates.erase(started_at - 1.0);
      }
    }
  }

  return baseline;
}

void
Visualizer::compute_lengths()
{

  /*
   * Step 1: Compute field heights
   */
  this->field_height.resize(this->instance.resource_count());
  this->img_height = 0.0;

  for (unsigned int res_id = 0 ; res_id < this->instance.resource_count() ; ++res_id) {
    this->baselines[res_id] = this->arrange_heights(res_id);

    double max_pos = 0.0;
    for (unsigned int job_id = 0 ; job_id < this->instance.job_count() ; ++job_id) {
      max_pos = std::max(max_pos, (double)this->baselines[res_id][job_id] + (this->instance.get_job(job_id).get_resource_usage(res_id) * RES_UNIT_HEIGHT));
    }

    // 2 * for ticks, 2 * for space
    this->field_height[res_id] = 4 * BASE_HEIGHT + max_pos;
    this->img_height += this->field_height[res_id];
  }


  /*
   * Step 2: Compute image width
   */
  this->latest_activity = 0;
  for (unsigned int job_id = 0; job_id < this->instance.job_count() ; ++job_id) {
    if (!this->solution.job_scheduled(job_id)) {
      continue;
    }
    this->latest_activity = (int)std::max((int)this->latest_activity, (int)(this->solution.get_start_time(job_id) + this->instance.get_job(job_id).get_duration()));
  }
  this->img_width = 4 * BASE_WIDTH + TIME_UNIT_WIDTH * (this->latest_activity + 1);

}

void
Visualizer::write(std::string filename)
{
  cairo_set_source_rgb (this->cr, 0, 0, 0);

  this->compute_lengths();

  this->surface = cairo_svg_surface_create(filename.c_str(), this->img_width, this->img_height);
  this->cr = cairo_create (surface);
  //cairo_translate(cr, 0.0, this->img_height);
  //cairo_scale(cr, 1.0, -1.0);

  for (unsigned int res_id = 0 ; res_id < this->instance.resource_count() ; ++res_id) {
    this->draw_field(res_id);
  }


  cairo_surface_flush(this->surface);
  cairo_surface_finish(this->surface);

  cairo_surface_destroy(this->surface);
  cairo_destroy(this->cr);
}
