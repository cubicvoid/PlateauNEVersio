#pragma once

#include "Controls.hpp"

#include "daisy_versio.h"

namespace util {
namespace daisy {

using ::daisy::DaisyVersio;

class VersioState {
public:
  void Init(const DaisyVersio &hw) {

    for (int i = 0; i < DaisyVersio::KNOB_LAST; i++) {
      knobs[i].Init(hw.AudioCallbackRate(), hw.GetKnobValue(i));
    }
  }

  void Process(const DaisyVersio &hw);

  const SmoothedKnob &Knob(DaisyVersio::AV_KNOBS knob) const {
    return knobs[knob];
  }

  const Switch &Tap() const { return tap; }
  const Switch3Pos &TopSwitchPos() const { return topSwitchPos; }
  const Switch3Pos &BottomSwitchPos() const { return bottomSwitchPos; }

private:
  SmoothedKnob knobs[DaisyVersio::KNOB_LAST];
  Switch tap;
  Switch3Pos topSwitchPos;
  Switch3Pos bottomSwitchPos;
};

} // namespace daisy

} // namespace util