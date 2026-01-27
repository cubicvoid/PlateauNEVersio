#pragma once

#include "Core.hpp"

namespace campestria {

class AudioState {
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
