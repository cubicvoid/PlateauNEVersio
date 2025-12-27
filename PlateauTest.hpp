#pragma once

#include <algorithm>

#include "signalsmith/delay.h"
#include "signalsmith/envelopes.h"

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

struct LimiterAttackHoldRelease {
  double limit = 0.85;
  double attackMs = 10;
  double holdMs = 0;
  double releaseMs = 600;

  signalsmith::envelopes::PeakHold<double> peakHold{0};
  signalsmith::envelopes::BoxStackFilter<double> smoother{0};
  // We don't need fractional delays, so this could be nearest-sample
  signalsmith::delay::Delay<double> delay;
  ExponentialRelease release; // see the previous example code

  int attackSamples = 0;
  void configure(double sampleRate) {
    attackSamples = attackMs * 0.001 * sampleRate;
    int holdSamples = holdMs * 0.001 * sampleRate;
    double releaseSamples = releaseMs * 0.001 * sampleRate;
    release = ExponentialRelease(releaseSamples);

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

    return smoother(release.step(-peakHold(-maxGain)));
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

struct KnobOnePoleFilter {
  double tmp = 0.;

  KnobOnePoleFilter() { inline double processLowpass(const double &x); }

  double processLowpass(const double &x) {
    tmp = 0.0005 * x + 0.9995 * tmp;
    return tmp;
  }
};

struct PopFilter {
  double tmp = 0.;

  PopFilter() { inline double processLowpass(const double &x); }

  double processLowpass(const double &x) {
    tmp = 0.01 * x + 0.99 * tmp;
    return tmp;
  }
};
