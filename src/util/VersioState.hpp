#pragma once

#include "Controls.hpp"

#include "daisy_versio.h"

namespace util {
namespace daisy {

using ::daisy::DaisyVersio;

class VersioState {
public:
  // Initialize specifying how often the Refresh method will be called
  // per second.
  void Init(const DaisyVersio &hw, uint32_t refreshFreq) {
    for (int i = 0; i < DaisyVersio::KNOB_LAST; i++) {
      knobs[i].Init(refreshFreq, hw.GetKnobValue(i));
    }
  }

  void Refresh(const DaisyVersio &hw);

  const SmoothedKnob &Knob(DaisyVersio::AV_KNOBS knob) const {
    return knobs[knob];
  }

  const Switch &Tap() const { return tap; }
  const Switch3 &TopSwitch() const { return topSwitch; }
  const Switch3 &BottomSwitch() const { return bottomSwitch; }

private:
  SmoothedKnob knobs[DaisyVersio::KNOB_LAST];
  Switch tap;
  Switch3 topSwitch;
  Switch3 bottomSwitch;
};

} // namespace daisy

} // namespace util