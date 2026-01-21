#pragma once

#include "Core.hpp"
#include "Settings.hpp"

namespace campestria {

class Parameters {
public:
  Parameters(daisy::PersistentStorage<Settings> &storage) : storage_(storage) {}

  const int GAIN_MODE_COUNT = 11;

  double wet = 0.5;

  double decay = 0.877465;

  double modSpeed = 0.0;
  double modDepth = 0.;
  double modShape = 0.;

  double preDelay = 0.;

  double timeScale = 1.0;

  double diffusion = 1.;

  double inputAmplification = 0.0;
  double outputAmplification = 0.0;

  double inputDampLow = 0.;
  double inputDampHigh = 0.;

  double reverbDampLow = 0.;
  double reverbDampHigh = 0.;

  bool freeze = false;

  uint32_t GainMode() const { return storage_.GetSettings().gainMode; }
  void SetGainMode(uint32_t mode) {
    storage_.GetSettings().gainMode = mode % GAIN_MODE_COUNT;
    storage_.Save();
  }

private:
  daisy::PersistentStorage<Settings> &storage_;
};

} // namespace campestria