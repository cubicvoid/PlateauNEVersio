#pragma once

#include "daisy_seed.h"

namespace util {
namespace daisy {

enum class Switch3Pos { CENTER, LEFT, RIGHT };

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
  void Debounce(const ::daisy::Switch &hwSwitch);

  bool Pressed() const { return last_edge_ == EdgeType::RISING; }
  bool RisingEdge() const { return updated_ && last_update_ == rise_time_; }
  bool FallingEdge() const { return updated_ && last_update_ == fall_time_; }

  float SecondsSinceLastPress() const {
    return static_cast<float>(last_update_ - rise_time_) * 0.0001;
  }

private:
  uint32_t last_update_ = 0;
  bool updated_ = false;
  uint8_t state_ = 0;

  enum class EdgeType { NONE, RISING, FALLING };
  EdgeType last_edge_ = EdgeType::NONE;
  uint32_t rise_time_ = 0;
  uint32_t fall_time_ = 0;
};

class Switch3 {
public:
  Switch3Pos Position() const { return pos; }

  void Refresh(const ::daisy::Switch3 &sw) {
    pos = static_cast<Switch3Pos>(sw.Read());
  }

private:
  Switch3Pos pos = Switch3Pos::CENTER;
};

class TriggerInput {
public:
  void Reset() { value = false; }
  void Set() { value = true; }
  bool Value() const { return value; }

private:
  bool value = false;
}

} // namespace daisy
} // namespace util