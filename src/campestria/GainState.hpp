#pragma once

#include "Core.hpp"
#include "Distortion.hpp"
#include "Parameters.hpp"

namespace campestria {

class GainState {
public:
  void Init() {
    bogLimiter.init();
    softerLimiterLeft.Init(hw.AudioSampleRate());
    softerLimiterRight.Init(hw.AudioSampleRate());
  }

  void gainControl(double *left, double *right);

  void Apply(const Parameters &params);

private:
  uint32_t mode = 0;
  double gainMod = 0;

  bogaudio::Lmtr bogLimiter;

  LimiterAttackHoldRelease softerLimiterLeft;
  LimiterAttackHoldRelease softerLimiterRight;

  RippedSpeakerFilter rippedSpeakerLeft;
  RippedSpeakerFilter rippedSpeakerRight;

  // Fast hyperbolic tangent function.
  void hardLimiter(double *x, double *y, float thresholdDb = -24.0f) {
    bogLimiter.engine.thresholdDb = thresholdDb;
    bogLimiter.processChannel(*x, *y, *x, *y);
  }
};

} // namespace campestria