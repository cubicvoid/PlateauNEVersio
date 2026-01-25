#pragma once

#include "ControlState.hpp"
#include "Core.hpp"
#include "Distortion.hpp"
#include "GainState.hpp"
#include "Parameters.hpp"
#include "ReverbState.hpp"
#include "Settings.hpp"
#include "Utility.hpp"

#include <algorithm>

namespace campestria {

class State {
public:
  uint32_t startTime = daisy::System::GetNow();

  bool awaitingConfirmation = false;

  bool bufferClearPending = false;

  float rmsLeftInput = 0.0f;
  float rmsRightInput = 0.0f;

  float rmsLeftOutput = 0.0f;
  float rmsRightOutput = 0.0f;
};

} // namespace campestria
