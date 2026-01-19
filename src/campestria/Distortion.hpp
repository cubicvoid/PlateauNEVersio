#pragma once

#include "Core.hpp"

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

} // namespace campestria