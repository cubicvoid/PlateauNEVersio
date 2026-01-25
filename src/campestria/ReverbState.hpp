#pragma once

#include "Core.hpp"
#include "Parameters.hpp"

namespace campestria {

class ReverbState {
public:
  ReverbState() : reverb(32000, 16, 4.0) {}

  void Init(float audioSampleRate) {
    reverb.setSampleRate(hw.AudioSampleRate());

    // Clear memory for reverb internal buffers
    for (int i = 0; i < 50; i++) {
      for (int j = 0; j < 144000; j++) {
        sdramData[i][j] = 0.;
      }
    }
  }

  void Apply(const Parameters &params) {
    reverb.setTimeScale(params.timeScale);
    reverb.setPreDelay(params.preDelay);
    reverb.setDecay(params.decay);
    reverb.setTankModSpeed(params.modSpeed);
    reverb.setTankModShape(params.modShape);
    reverb.setTankModDepth(params.modDepth);
    reverb.setInputFilterHighCutoffPitch(params.inputDampHigh);
    reverb.setTankFilterHighCutFrequency(params.reverbDampHigh);
    reverb.setInputFilterLowCutoffPitch(params.inputDampLow);
    reverb.setTankFilterLowCutFrequency(params.reverbDampLow);
    reverb.setTankDiffusion(params.diffusion);
    reverb.enableInputDiffusion(params.diffusion != 0);
    reverb.freeze(params.freeze);
  }

  void Clear() { reverb.clear(); }

  void Process(double leftInput, double rightInput) {
    reverb.process(leftInput, rightInput);
  }

  double GetLeftOutput() { return reverb.getLeftOutput(); }

  double GetRightOutput() { return reverb.getRightOutput(); }

private:
  Dattorro reverb;
};
} // namespace campestria