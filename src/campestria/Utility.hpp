#pragma once

#include <algorithm>

namespace campestria {

struct ExponentialRelease {
  double releaseSlew;
  double output = 1;

  ExponentialRelease(double releaseSamples = 1280) {
    // The exact value is `1 - exp(-1/releaseSamples)`
    // but this is a decent approximation
    releaseSlew = 1 / (releaseSamples + 1);
  }

  double step(double input) {
    // Move towards input
    output += (input - output) * releaseSlew;
    output = std::min(output, input);
    return output;
  }
};

struct OnePoleFilter {
  double coeff;
  double value;

  OnePoleFilter(double slew = 1.0, double initialValue = 0.0)
      : coeff(slew), value(initialValue) {}

  double process(const double &x) {
    value += coeff * (x - value);
    return value;
  }
};

struct PopFilter : public OnePoleFilter {
  PopFilter() : OnePoleFilter(0.01) {}
};

struct GateFilter : public OnePoleFilter {
  GateFilter() : OnePoleFilter(0.15) {}
};

struct RippedSpeakerFilter {
  static constexpr unsigned int HOLD_SAMPLES = 32;
  GateFilter gateFilter;
  uint32_t rippedCountdown = 0;
  double threshold_ = 1.0;

  double process(double x) {
    if (fabs(x) > threshold_) {
      rippedCountdown = HOLD_SAMPLES;
    }
    double coeff;
    if (rippedCountdown > 0) {
      --rippedCountdown;
      coeff = gateFilter.process(0);
    } else {
      coeff = gateFilter.process(1);
    }
    return coeff * x;
  }

  void SetThreshold(double threshold) { threshold_ = threshold; }
};

} // namespace campestria