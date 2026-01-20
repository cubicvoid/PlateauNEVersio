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

// Wrapper struct around the libDaisy-provided Switch class, to implement
// smoothing and rising/falling edge behavior that is easier to work with.
// Changes to baseline logic:
// - Does not report multiple rising edges without a falling edge between
//  them, or vice versa.
// - When a rising edge is reported, Pressed() returns true.
// - A gap in the hold signal too short to hit the falling edge threshold
//   (8 ms) does not reset the hold time.
// - Rising / falling edges are reported after the button state has been
//   stable for 8 ms instead of 7 ms.
// Overall: Pressed() returns false until the 8th consecutive on state,
// at which point a rising edge is reported, and Pressed() then returns
// true until the 8th consecutive off state, at which point a falling
// edge is reported.
class Switch {
public:
  Switch(daisy::Switch *raw) : raw_(raw) {}

  void Debounce();

  bool Pressed() { return last_edge_ == EdgeType::RISING; }
  bool RisingEdge() { return updated_ && last_update_ == rise_time_; }
  bool FallingEdge() { return updated_ && last_update_ == fall_time_; }

  float SecondsSinceLastPress() {
    return static_cast<float>(last_update_ - rise_time_) * 0.0001;
  }

private:
  daisy::Switch *raw_;

  uint32_t last_update_ = 0;
  bool updated_ = false;
  uint8_t state_ = 0;

  enum class EdgeType { NONE, RISING, FALLING };
  EdgeType last_edge_ = EdgeType::NONE;
  uint32_t rise_time_ = 0;
  uint32_t fall_time_ = 0;
};

// The logical state of the input parameters from the DaisyVersio driver after
// basic clipping and smoothing.
struct ControlState {
  SmoothedKnob knobs[DaisyVersio::KNOB_LAST];

  SwitchState topSwitch = SwitchState::CENTER;
  SwitchState bottomSwitch = SwitchState::CENTER;

  Switch tap;

  ControlState() : tap(&hw.tap) {}

  // Initialize specifying how often the Refresh method will be called
  // per second.
  void Init(uint32_t refreshFreq) {
    for (int i = 0; i < DaisyVersio::KNOB_LAST; i++) {
      knobs[i].Init(refreshFreq, hw.GetKnobValue(i));
    }
  }

  void Refresh() {
    hw.ProcessAnalogControls();
    for (int i = 0; i < DaisyVersio::KNOB_LAST; i++) {
      knobs[i].Refresh(hw.GetKnobValue(i));
    }
    topSwitch = static_cast<SwitchState>(hw.sw[0].Read());
    bottomSwitch = static_cast<SwitchState>(hw.sw[1].Read());
    tap.Debounce();
  }

  const SmoothedKnob &Knob(Knob k) { return knobs[static_cast<int>(k)]; }
};

} // namespace campestria
