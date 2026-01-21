#pragma once

#include "Core.hpp"
#include "Utility.hpp"

namespace campestria {

struct LimiterAttackHoldRelease {
  double limit = 0.85;
  double attackMs = 10;
  double holdMs = 0;
  double releaseMs = 600;

  signalsmith::envelopes::PeakHold<double> peakHold{0};
  signalsmith::envelopes::BoxStackFilter<double> smoother{0};
  // We don't need fractional delays, so this could be nearest-sample
  signalsmith::delay::Delay<double> delay;
  ExponentialRelease follower; // see the previous example code

  int attackSamples = 0;
  void Init(double sampleRate) {
    attackSamples = attackMs * 0.001 * sampleRate;
    int holdSamples = holdMs * 0.001 * sampleRate;
    double followerSamples = releaseMs * 0.001 * sampleRate;
    follower = ExponentialRelease(followerSamples);

    peakHold.resize(attackSamples + holdSamples);
    smoother.resize(attackSamples, 3);
    smoother.reset(1);

    delay.resize(attackSamples + 1);
  }
  int latencySamples() { return attackSamples; }

  inline double gain(const double &v) {
    double maxGain = 1;
    if (std::abs(v) > limit) {
      maxGain = limit / std::abs(v);
    }

    return smoother(follower.step(-peakHold(-maxGain)));
  }

  double sample(const double &v) {
    return delay.write(v).read(attackSamples) * gain(v);
  }
};

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

inline double saturation(double x) {
  return x * (27. + x * x) / (27. + 9. * x * x);
}

inline double hardClip(const double &x, double limit) {
  return std::max(std::min(x, limit), -limit);
}

} // namespace campestria