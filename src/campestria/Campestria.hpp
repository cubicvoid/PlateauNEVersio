#pragma once

#include "ControlState.hpp"
#include "Core.hpp"
#include "Distortion.hpp"
#include "Utility.hpp"

#include <algorithm>

namespace campestria {

inline void foldbackDistortion(double &x, double threshold) {
  if (x > threshold || x < -threshold) {
    x = std::fabs(std::fabs(std::fmod(x - threshold, threshold * 4)) -
                  threshold * 2) -
        threshold;
  }
}

inline double hardLimit50_(const double &x) {
  return (x > 0.50) ? 0.50 : ((x < -0.50) ? -0.50 : x);
}

inline double hardLimit77_8_(const double &x) {
  return (x > 0.778) ? 0.778 : ((x < -0.778) ? -0.778 : x);
}

inline double hardLimit100_(const double &x) {
  return (x > 1.) ? 1. : ((x < -1.) ? -1. : x);
}

inline double amp120_(const double &x) { return x * 1.2; }

} // namespace campestria