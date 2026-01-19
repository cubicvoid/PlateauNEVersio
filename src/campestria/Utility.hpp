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

}