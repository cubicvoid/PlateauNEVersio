#pragma once

#include "Core.hpp"

#include <algorithm>

namespace campestria {

using daisy::DaisyVersio;

// Snap-to-endpoints and smoothing on top of the raw values from the driver.
// We use this instead of just overriding the coefficients for the smoothing
// filters built into the ADC API because that would run before the endpoint
// clipping and we want the smoothing to happen after.
struct SmoothedKnob {
  double value;
  double rawValue_;

  bool moving = false;
  double coeff = 0.016;

  void Init(uint32_t sampleRate, float initialValue = 0.0f) {
    rawValue_ = value = initialValue;
    coeff = std::min(16.0f / sampleRate, 1.0f);
  }

  void Refresh(double rawValue) {
    moving = fabs(rawValue_ - rawValue) > 0.00025;
    rawValue_ = rawValue;
    const double scaled = rawValue * 1.01 - 0.005;
    const double clipped = std::max(std::min(scaled, 1.0), 0.0);
    value += coeff * (clipped - value);
    if (value <= 1.0e-030) {
      value = 0.0;
    }
  }
};

// The logical state of the input parameters from the DaisyVersio driver after
// basic clipping and smoothing.
struct ControlState {
  SmoothedKnob knobs[DaisyVersio::KNOB_LAST];

  SwitchState topSwitch = SwitchState::CENTER;
  SwitchState bottomSwitch = SwitchState::CENTER;

  // Initialize specifying how often the Refresh method will be called
  // per second.
  void Init(uint32_t refreshFreq) {
    for (int i = 0; i < DaisyVersio::KNOB_LAST; i++) {
      knobs[i].Init(refreshFreq, hw.GetKnobValue(i));
    }
  }
  void Refresh() {
    for (int i = 0; i < DaisyVersio::KNOB_LAST; i++) {
      knobs[i].Refresh(hw.GetKnobValue(i));
    }
  }

  const SmoothedKnob &Knob(Knob k) { return knobs[static_cast<int>(k)]; }
};

} // namespace campestria
