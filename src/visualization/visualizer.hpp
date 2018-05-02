#ifndef VISUALIZER_H
#define VISUALIZER_H

#include "instance/solution.hpp"

#include <cairo.h>

#define BASE_WIDTH 40
#define BASE_HEIGHT 40

#define RES_UNIT_HEIGHT 40
#define TIME_UNIT_WIDTH 40
#define TICK_SIZE 20

class Visualizer {
public:
  Visualizer(const Solution &solution);
  ~Visualizer();

  void write(std::string filename);

private:
  const Solution &solution;
  const Instance &instance;

  cairo_surface_t *surface;
  cairo_t *cr;

  std::map<unsigned int, std::map<unsigned int, double>> baselines;
  std::vector<double> field_height;

  int latest_activity;

  double img_height;
  double img_width;

  void compute_lengths();
  void draw_field(unsigned int res_id);
  std::map<unsigned int, double> arrange_heights(unsigned int res_id);
};

#endif
