#pragma once

#include "campestria/Core.hpp"
#include "campestria/Distortion.hpp"
#include "campestria/Parameters.hpp"

namespace campestria {

class GainState {
public:
  void Init(float audioSampleRate) {
    bogLimiter.init();
    softerLimiterLeft.Init(audioSampleRate);
    softerLimiterRight.Init(audioSampleRate);
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