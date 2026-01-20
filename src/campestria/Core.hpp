#pragma once

#include "daisy_versio.h"

#include "Bogaudio/Lmtr.hpp"
#include "Bogaudio/bogaudio.hpp"
#include "ValleyRackFree/Plateau/Dattorro.hpp"
#include "signalsmith/delay.h"
#include "signalsmith/envelopes.h"

namespace campestria {

enum class SwitchState { CENTER, LEFT, RIGHT };

enum class Knob {
  WET,
  MOD_SPEED,
  TONE,
  MOD_DEPTH,
  DECAY,
  TIME_SCALE,
  PRE_DELAY,
  LAST
};

extern daisy::DaisyVersio hw;

} // namespace campestria