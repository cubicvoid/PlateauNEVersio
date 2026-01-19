#include "Campestria.hpp"

#include "Bogaudio/Lmtr.hpp"
#include "Bogaudio/bogaudio.hpp"
#include "ValleyRackFree/Plateau/Dattorro.hpp"
#include "signalsmith/delay.h"
#include "signalsmith/envelopes.h"

using namespace daisy;
using namespace daisysp;
// using namespace bogaudio;

DaisyVersio hw;

struct SmoothedKnob {
  float prevValue;
  float value;
  float coeff = 0.016;
  void update(float x) {
    float scaled = x * 1.01f - 0.005f;
    float clipped = (scaled >= 1) * 1.0f + (scaled < 1 && scaled > 0) * scaled;
    value += (clipped - value) * coeff;
    if (value <= 1.0e-030) {
      value = 0.0f;
    }
  }
  bool isMoving() {
    return fabs(value - prevValue) > 0.005;
  }
};
namespace campestria {

struct OnePoleAudioBlockFilter {
  double coeff = 1.0;
  double value = 0.0;

  OnePoleAudioBlockFilter(double slewPerSample) : coeff(slewPerSample) {}

  double process(const double &x) {
    value += coeff * hw.AudioBlockSize() * (x - value);
    return value;
  }
};

struct ParameterSmoother : public OnePoleAudioBlockFilter {
  ParameterSmoother() : OnePoleAudioBlockFilter(0.0005) {}
};

struct PopFilter : public OnePoleAudioBlockFilter {
  PopFilter() : OnePoleAudioBlockFilter(0.01) {}
};

} // namespace campestria

// Persistence
struct Settings {
  int gainMode = 0;
  // int buttonMode;
  bool operator!=(const Settings &a) {
    return (a.gainMode != gainMode);
    // or (a.buttonMode != buttonMode);
  }
};
Settings &operator*(const Settings &settings) { return *settings; }
PersistentStorage<Settings> storage(hw.seed.qspi);

campestria::LimiterAttackHoldRelease softerLimiterLeft;
campestria::LimiterAttackHoldRelease softerLimiterRight;

bogaudio::Lmtr bogLimiter;

const double minus18dBGain = 0.12589254;
const double minus20dBGain = 0.1;

struct Parameters {
  double wet = 0.5;

  double decay = 0.877465;

  double modDepth = 0.;
  double preDelay = 0.;
  
  double timeScale = 1.0;

  double diffusion = 1.;
  double tempDiffusion = 1.;

  double inputDampLow = 0.;
  double inputDampHigh = 0.;

  double reverbDampLow = 0.;
  double reverbDampHigh = 0.;

  double previousInputDampLow = 0.;
  double previousInputDampHigh = 0.;

  double previousReverbDampLow = 0.;
  double previousReverbDampHigh = 0.;

  uint32_t GainMode() { return storage.GetSettings().gainMode; }
  void SetGainMode(uint32_t mode) {
    storage.GetSettings().gainMode = mode;
    storage.Save();
  }
};

Parameters params;

unsigned int gainModeLedCountdown = 0;
const float gainModeLedDisplayTime = 1.0;

struct Samples {
  double leftInput = 0.;
  double rightInput = 0.;
  double leftOutput = 0.;
  double rightOutput = 0.;
};

Samples samples;

struct State {

  // input volume modifier is currently unused
  // double inputVolumeModifier = 1.;
  // double tempInputVolumeModifier = 1.;
  unsigned int buttonHoldTimer = 0;
  unsigned int buttonOffTimer = 0;

  double lockedModDepthValue = 0.;
  double previousModDepthKnobValue = 0.;
  bool modDepthKnobIsMoving = false;

  bool lockModDepthTo3_125_ = false;
};

State state;

double volumeChange = 0.;

unsigned int holdCount = 0;

bool confirmationSequence = false;
bool confirmationSequenceOne = false;
bool confirmationSequenceTwo = false;
unsigned int confirmationSequenceCounter = 0;
unsigned int confirmationSequenceTimer = 0;

enum class ButtonMode { Gain, Clear, Freeze };
ButtonMode buttonMode = ButtonMode::Gain;

unsigned int toneKnobLedTimer = 32001;
const unsigned int toneKnobLedOnTime = 32000;
bool toneKnobIsMoving = false;
double previousToneKnobZeroLockValue = 0.;

bool leds = true;

Dattorro reverb(32000, 16, 4.0);

bool diffusionEnabled = true;

double outputAmplification = 0.0;
double tempOutputAmplification = outputAmplification;

double inputAmplification = 0.0;
double tempInputAmplification = inputAmplification;

enum class SwitchState { Left, Center, Right };

enum class Knob {
  WET, MOD_SPEED, TONE, MOD_DEPTH, DECAY, TIME_SCALE, PRE_DELAY, LAST
};

struct ControlState {
  SmoothedKnob knobs[DaisyVersio::KNOB_LAST];

  SwitchState topSwitch = SwitchState::Left;
  SwitchState bottomSwitch = SwitchState::Right;

  void update() {
    for (int i = 0; i < DaisyVersio::KNOB_LAST; i++) {
      knobs[i].update(hw.GetKnobValue(i));
    }

  }

  float knob(Knob k) {
    int index = static_cast<int>(k);
    return knobs[index].value;
  }
};

ControlState controlState;

unsigned int saveTimer = 0;
bool saveTrigger = false;
unsigned int saveTime = 32000;

bool clear = false;

bool shapeSet = true;

unsigned int lockModDepthTime = 320000;
unsigned int bufferClearTriggerWindow = 32000;

struct LedTimer {
  // unsigned int genericLedOnTime = 32000;
  unsigned int genericLedCountdown = 0;
  const unsigned int genericLedTimeout = 32001;
};

LedTimer ledTimer;

bool freeze = false;

inline void saturation(double &x) {
  x = x * (27. + x * x) / (27. + 9. * x * x);
}

// Fast hyperbolic tangent function.
inline void hardLimiter(double &x, double &y, float thresholdDb = -24.0f) {
  bogLimiter.engine.thresholdDb = thresholdDb;
  bogLimiter.processChannel(x, y, x, y);
}

inline void softLimiter(double &x, double &y) {
  x = softerLimiterLeft.sample(x);
  y = softerLimiterRight.sample(y);
}

double hardClipGain = 0.85;
inline double hardClip(const double &x) {
  return (x > hardClipGain) ? hardClipGain
                            : ((x < -hardClipGain) ? -hardClipGain : x);
}

unsigned int holdSamples = 32;
unsigned int rippedCountLeft = 0;
double leftValue = 1.;
unsigned int rippedCountRight = 0;
double rightValue = 1.;
double smoothing = 0.85;

struct GateFilter {
  double tmp = 0.;

  GateFilter() { inline double processLowpass(const double &x); }

  double processLowpass(const double &x) {
    tmp = (1 - smoothing) * x + smoothing * tmp;
    return tmp;
  }
};

GateFilter leftGateFilter;
GateFilter rightGateFilter;

inline void rippedSpeakerLeft(double &x, double threshold) {
  if (rippedCountLeft < holdSamples) {
    ++rippedCountLeft;
    leftValue = 0.;
  } else if (rippedCountLeft == holdSamples) {
    rippedCountLeft = holdSamples + 1;
    leftValue = 1.;
  }
  if (x > threshold || x < -threshold) {
    rippedCountLeft = 0;
    leftValue = 0.;
  }
  x *= leftGateFilter.processLowpass(leftValue);
}

inline void rippedSpeakerRight(double &x, double threshold) {
  if (rippedCountRight < holdSamples) {
    ++rippedCountRight;
    rightValue = 0.;
  } else if (rippedCountRight == holdSamples) {
    rippedCountRight = holdSamples + 1;
    rightValue = 1.;
  }
  if (x > threshold || x < -threshold) {
    rippedCountRight = 0;
    rightValue = 0.;
  }
  x *= rightGateFilter.processLowpass(rightValue);
}

// campestria::KnobOnePoleFilter knobLPF[DaisyVersio::LED_LAST];
//  campestria::KnobOnePoleFilter mixKnobLPF;
//  campestria::KnobOnePoleFilter modDepthKnobLPF;
//  campestria::KnobOnePoleFilter preDelayKnobLPF;
//  campestria::KnobOnePoleFilter timeScaleLPF;
//  campestria::KnobOnePoleFilter toneKnobLPF;
//  campestria::KnobOnePoleFilter toneKnobZeroLockLPF;
//  campestria::KnobOnePoleFilter decayKnobLPF;

inline void checkIfModDepthKnobIsMoving(double currentValue) {
  if (((currentValue - state.previousModDepthKnobValue) > 0.001) or
      ((currentValue - state.previousModDepthKnobValue) < -0.001)) {
    state.previousModDepthKnobValue = currentValue;
    state.modDepthKnobIsMoving = true;
  } else {
    state.modDepthKnobIsMoving = false;
  }
}

// These pointers are necessary to speed up the code, otherwise severe crackling
// occurs.
inline void setLEDs(const double &w, const double &x, const double &y,
                    const double &z, bool update = false) {
  hw.SetLed(0, w, 0.0f, 0.0f);
  hw.SetLed(1, x, 0.0f, 0.0f);
  hw.SetLed(2, y, 0.0f, 0.0f);
  hw.SetLed(3, z, 0.0f, 0.0f);
  if (update) {
    hw.UpdateLeds();
  }
}

inline void prepareLeds(const double &w, const double &x, const double &y,
                        const double &z) {
  hw.SetLed(0, w, 0.0f, 0.0f);
  hw.SetLed(1, x, 0.0f, 0.0f);
  hw.SetLed(2, y, 0.0f, 0.0f);
  hw.SetLed(3, z, 0.0f, 0.0f);
}

inline void prepareGenericLed() {
  if (ledTimer.genericLedCountdown > 0) {
    float x = (--ledTimer.genericLedCountdown > 0);
    prepareLeds(x, x, x, x);
  }
}

bool gateState = false;
inline void checkGate() { gateState = !hw.gate.State(); }

inline void checkSwitches() {
  controlState.topSwitch = static_cast<SwitchState>(hw.sw[0].Read());
  controlState.bottomSwitch = static_cast<SwitchState>(hw.sw[1].Read());
}

inline void ProcessSwitches() {

  if (controlState.topSwitch == SwitchState::Right) {
    if (controlState.bottomSwitch == SwitchState::Center) {
      params.inputDampHigh = controlState.knob(Knob::TONE);
      if (((params.inputDampHigh - params.previousInputDampHigh) < 0.01) and
          ((params.inputDampHigh - params.previousInputDampHigh) > -0.01)) {
        params.previousInputDampHigh = params.inputDampHigh;
        reverb.setInputFilterHighCutoffPitch(10. -
                                             (10. * params.inputDampHigh));
        if (toneKnobIsMoving) {
          toneKnobLedTimer = 0;
        }
        if (toneKnobLedTimer < toneKnobLedOnTime) {
          ++toneKnobLedTimer;
          prepareLeds(0., params.inputDampHigh, 0., 0.);
        } else if (toneKnobLedTimer == toneKnobLedOnTime) {
          toneKnobLedTimer = toneKnobLedOnTime + 1;
          prepareLeds(0., 0., 0., 0.);
        }
      }
    } else if (controlState.bottomSwitch == SwitchState::Right) {
      params.reverbDampHigh = controlState.knob(Knob::TONE);
      if (((params.reverbDampHigh - params.previousReverbDampHigh) < 0.01) and
          ((params.reverbDampHigh - params.previousReverbDampHigh) > -0.01)) {
        params.previousReverbDampHigh = params.reverbDampHigh;
        reverb.setTankFilterHighCutFrequency(10. -
                                             (10. * params.reverbDampHigh));
        if (toneKnobIsMoving) {
          toneKnobLedTimer = 0;
        }
        if (toneKnobLedTimer < toneKnobLedOnTime) {
          ++toneKnobLedTimer;
          prepareLeds(0., 0., 0., params.reverbDampHigh);
        } else if (toneKnobLedTimer == toneKnobLedOnTime) {
          toneKnobLedTimer = toneKnobLedOnTime + 1;
          prepareLeds(0., 0., 0., 0.);
        }
      }
    }
  } else if (controlState.topSwitch == SwitchState::Center) {
    if (controlState.bottomSwitch == SwitchState::Center) {
      params.inputDampLow = controlState.knob(Knob::TONE);
      if (((params.inputDampLow - params.previousInputDampLow) < 0.01) and
          ((params.inputDampLow - params.previousInputDampLow) > -0.01)) {
        params.previousInputDampLow = params.inputDampLow;
        reverb.setInputFilterLowCutoffPitch(params.inputDampLow * 10.);
        if (toneKnobIsMoving) {
          toneKnobLedTimer = 0;
        }
        if (toneKnobLedTimer < toneKnobLedOnTime) {
          ++toneKnobLedTimer;
          prepareLeds(params.inputDampLow, 0., 0., 0.);
        } else if (toneKnobLedTimer == toneKnobLedOnTime) {
          toneKnobLedTimer = toneKnobLedOnTime + 1;
          prepareLeds(0., 0., 0., 0.);
        }
      }
    } else if (controlState.bottomSwitch == SwitchState::Right) {
      params.reverbDampLow = controlState.knob(Knob::TONE);
      if (((params.reverbDampLow - params.previousReverbDampLow) < 0.01) and
          ((params.reverbDampLow - params.previousReverbDampLow) > -0.01)) {
        params.previousReverbDampLow = params.reverbDampLow;
        reverb.setTankFilterLowCutFrequency(params.reverbDampLow * 10.);
        if (toneKnobIsMoving) {
          toneKnobLedTimer = 0;
        }
        if (toneKnobLedTimer < toneKnobLedOnTime) {
          ++toneKnobLedTimer;
          prepareLeds(0., 0., params.reverbDampLow, 0.);
        } else if (toneKnobLedTimer == toneKnobLedOnTime) {
          toneKnobLedTimer = toneKnobLedOnTime + 1;
          prepareLeds(0., 0., 0., 0.);
        }
      }
    }
  }

  if (controlState.bottomSwitch == SwitchState::Left) {
    switch (controlState.topSwitch) {
    case SwitchState::Left:
      params.tempDiffusion = controlState.knob(Knob::TONE);
      if (((params.tempDiffusion - params.diffusion) < 0.01) and
          ((params.tempDiffusion - params.diffusion) > -0.01)) {
        params.diffusion = params.tempDiffusion;
        if (params.diffusion == 0.) {
          if (diffusionEnabled) {
            diffusionEnabled = false;
            reverb.enableInputDiffusion(diffusionEnabled);
          }
        } else {
          if (!diffusionEnabled) {
            diffusionEnabled = true;
            reverb.enableInputDiffusion(diffusionEnabled);
          }
          reverb.setTankDiffusion(params.diffusion * 0.7);
        }
        if (toneKnobIsMoving) {
          toneKnobLedTimer = 0;
        }
        if (toneKnobLedTimer < toneKnobLedOnTime) {
          ++toneKnobLedTimer;
          prepareLeds(params.diffusion, params.diffusion, params.diffusion,
                      params.diffusion);
        } else if (toneKnobLedTimer == toneKnobLedOnTime) {
          toneKnobLedTimer = toneKnobLedOnTime + 1;
          prepareLeds(0., 0., 0., 0.);
        }
      }
      break;
    case SwitchState::Center:
      tempInputAmplification = controlState.knob(Knob::TONE);
      if (((tempInputAmplification - inputAmplification) < 0.01) and
          ((tempInputAmplification - inputAmplification) > -0.01)) {
        inputAmplification = tempInputAmplification;
        if (toneKnobIsMoving) {
          toneKnobLedTimer = 0;
        }
        if (toneKnobLedTimer < toneKnobLedOnTime) {
          ++toneKnobLedTimer;
          prepareLeds(inputAmplification, inputAmplification,
                      inputAmplification, inputAmplification);
        } else if (toneKnobLedTimer == toneKnobLedOnTime) {
          toneKnobLedTimer = toneKnobLedOnTime + 1;
          prepareLeds(0., 0., 0., 0.);
        }
      }
      break;
    case SwitchState::Right:
      tempOutputAmplification = controlState.knob(Knob::TONE);
      if (((tempOutputAmplification - outputAmplification) < 0.01) and
          ((tempOutputAmplification - outputAmplification) > -0.01)) {
        outputAmplification = tempOutputAmplification;
        if (toneKnobIsMoving) {
          toneKnobLedTimer = 0;
        }
        if (toneKnobLedTimer < toneKnobLedOnTime) {
          ++toneKnobLedTimer;
          prepareLeds(outputAmplification, outputAmplification,
                      outputAmplification, outputAmplification);
        } else if (toneKnobLedTimer == toneKnobLedOnTime) {
          toneKnobLedTimer = toneKnobLedOnTime + 1;
          prepareLeds(0., 0., 0., 0.);
        }
      }
      break;
    }
  }
}

inline void processButton_GainMode() {
  if (hw.tap.Pressed()) {
    if (state.buttonHoldTimer < 5 * 160000) {
      gainModeLedCountdown = gainModeLedDisplayTime * hw.AudioCallbackRate();
    }
  }
}
void processButton_ClearMode() {}
void processButton_FreezeMode() { freeze = hw.tap.Pressed(); }

// Button has three modes, gain control, buffer clear, freeze.
// Hold down the button for 10 seconds, when the LEDs come on
// press again within one second to confirm mode change. If x gain control
// mode not confirming will lock mod depth to 3.125%. In buffer clear mode, a
// rising edge will trigger the buffers to clear. In freeze mode, holding the
// button will freeze the buffers.
inline void ProcessButton() {

  if (hw.tap.Pressed()) {
    state.buttonHoldTimer++;
  } else {
    state.buttonHoldTimer = 0;
  }

  switch (buttonMode) {
  case ButtonMode::Gain:
    processButton_GainMode();
    break;
  case ButtonMode::Clear:
    processButton_ClearMode();
    break;
  case ButtonMode::Freeze:
    processButton_FreezeMode();
    break;
  }
  if (hw.SwitchPressed()) {
    ++state.buttonHoldTimer;
    if (state.buttonHoldTimer ==
        10 * static_cast<unsigned int>(hw.AudioCallbackRate())) {
      ledTimer.genericLedCountdown = ledTimer.genericLedTimeout;
    }
    if (state.buttonHoldTimer ==
        11 * static_cast<unsigned int>(hw.AudioCallbackRate())) {
      if (buttonMode == ButtonMode::Gain) {
        state.lockedModDepthValue = params.modDepth;
        if (state.lockModDepthTo3_125_) {
          shapeSet = false;
          state.lockModDepthTo3_125_ = false;
        } else {
          state.lockModDepthTo3_125_ = true;
        }
      }
    }
    if (hw.tap.RisingEdge()) {
      if (confirmationSequence) {
        if (ledTimer.genericLedCountdown > 0) {
          if (buttonMode == ButtonMode::Freeze) {
            buttonMode = ButtonMode::Gain;
            // saveData();
          } else {
            buttonMode =
                static_cast<ButtonMode>(static_cast<int>(buttonMode) + 1);
            // saveData();
          }
          confirmationSequence = false;
        } else {
          // This might be redundant.
          confirmationSequence = false;
        }
      }
      if (buttonMode == ButtonMode::Clear) {
        ledTimer.genericLedCountdown = ledTimer.genericLedTimeout;
        clear = true;
      }
    }
  } else {

    if (hw.tap.FallingEdge()) {
      if ((state.buttonHoldTimer > 10 * hw.AudioCallbackRate()) and
          (state.buttonHoldTimer < 11 * hw.AudioCallbackRate())) {
        confirmationSequence = true;
      }
      if (buttonMode == ButtonMode::Gain) {
        if (state.buttonHoldTimer < 0.25f * hw.AudioCallbackRate()) {
          gainModeLedCountdown =
              gainModeLedDisplayTime * hw.AudioCallbackRate();
          params.SetGainMode(params.GainMode() + 1);
        }
      }
    }
    state.buttonHoldTimer = 0;
  }
}

inline void interpolatingDelayHold() {
  if (holdCount < 192000)
    ++holdCount;
  else
    hold = 1.;
}
inline float SnappedToUnitInterval(float v) {
  return (v < 0.01f) ? 0.0f : (v > 0.99) ? 1.0f : (v - 0.01f) / 0.98f;
}

void ProcessTimeScale() {
  params.timeScale = controlState.knob(Knob::TIME_SCALE);
 
}


void ProcessMix() {
  params.wet = controlState.knob(Knob::WET);
}

void ProcessModSpeed() {
  // Unlike mix, mod speed need not be locked to zero. Mod speed is not
  // succeptible to noise
  reverb.setTankModSpeed(0.5 + (controlState.knob(Knob::MOD_SPEED) * 100.));
}

void ProcessModDepth() {
  params.modDepth = controlState.knob(Knob::MOD_DEPTH);

  // Ability to lock mod depth to the equivalent default 3.125% of VCV rack
  if (state.lockModDepthTo3_125_) {
    reverb.setTankModShape(0.001 + (params.modDepth * 0.998));
    reverb.setTankModDepth(0.5 + (state.lockedModDepthValue * 15.5));
  } else {
    if (!shapeSet) {
      reverb.setTankModShape(0.5);
      shapeSet = true;
    }
    reverb.setTankModDepth(params.modDepth * 16.);
  }
}

inline void processDecay() {
  float scaledKnob = 0.0001 + 0.7999 * (1 - controlState.knob(Knob::DECAY));
  params.decay = 1 - (scaledKnob * scaledKnob);

}
void ProcessPreDelay() {
  params.preDelay = controlState.knob(Knob::PRE_DELAY);
  
}

inline void ApplyAllParameters() {
 reverb.setTimeScale(params.timeScale);
   reverb.setPreDelay(params.preDelay);
  reverb.setDecay(params.decay);
}

inline void ProcessAllParameters() {
  ProcessButton();

  ProcessTimeScale();
  ProcessMix();
  ProcessModSpeed();
  ProcessModDepth();
  ProcessPreDelay();
  ProcessSwitches();

  ApplyAllParameters();
}

// inline void saveCounterAudioRate() {
//     if(saveTimer < saveTime) {
//         ++saveTimer;
//         saveTrigger = false;
//     } else {
//         saveTimer = 0;
//         saveTrigger = true;
//     }
// }

// Is mutating the output this way a mortal sin?
inline void gainControl(double &leftOutput, double &rightOutput) {
  double saturatedLeft = leftOutput;
  double saturatedRight = rightOutput;
  double mix = 1. - ((1. - outputAmplification) * (1. - outputAmplification) *
                     (1. - outputAmplification));
  switch (params.GainMode()) {
  case 0: {
    // Regular soft limiter. Rarely clips. Lower limit threshold by turning
    // tone knob up output dynamic setting selected.
    softerLimiterLeft.limit = (0.85 - (outputAmplification * 0.85));
    softerLimiterRight.limit = (0.85 - (outputAmplification * 0.85));
    break;
  }
  case 1: {
    // Same clipper as VCV rack. Lower clip threshold by turning tone knob up
    // with output dynamic setting selected.
    softerLimiterLeft.limit = 0.85;
    softerLimiterRight.limit = 0.85;
    hardClipGain = (1. - outputAmplification) * (1. - outputAmplification);
    leftOutput = hardClip(leftOutput);
    rightOutput = hardClip(rightOutput);
    // modifier = (-9.8 / (-40.5 + (40. * outputAmplification))) + 0.758;
    double modifier = 1. + ((1. / (hardClipGain + 0.000000001)) *
                            (1.2 - outputAmplification));
    leftOutput *= modifier;
    rightOutput *= modifier;

  } break;
  case 2: {
    // Same as last but with saturation
    softerLimiterLeft.limit = 0.85;
    softerLimiterRight.limit = 0.85;
    hardClipGain = (1. - outputAmplification) * (1. - outputAmplification);
    leftOutput = hardClip(leftOutput);
    rightOutput = hardClip(rightOutput);
    double modifier = 1. + ((1. / (hardClipGain + 0.000000001)) *
                            (1.2 - outputAmplification));
    leftOutput *= modifier;
    rightOutput *= modifier;

    saturatedLeft = leftOutput;
    saturatedRight = rightOutput;
    saturatedLeft *= 1. + outputAmplification * 20.;
    saturatedRight *= 1. + outputAmplification * 20.;
    saturation(saturatedLeft);
    saturation(saturatedRight);
    saturatedLeft *= 1 - 1.6 * mix + 0.83 * mix * mix;
    saturatedRight *= 1 - 1.6 * mix + 0.83 * mix * mix;

    leftOutput = leftOutput * (1. - mix) + saturatedLeft * mix;
    rightOutput = rightOutput * (1. - mix) + saturatedRight * mix;
    hardLimiter(leftOutput, rightOutput);

  } break;
  case 3: {
    // Just saturation. Control gain going into saturation with tone knob
    // output dynamic setting
    softerLimiterLeft.limit = 0.85;
    softerLimiterRight.limit = 0.85;
    saturatedLeft = leftOutput;
    saturatedRight = rightOutput;
    saturatedLeft *= 1. + outputAmplification * 20.;
    saturatedRight *= 1. + outputAmplification * 20.;
    saturation(saturatedLeft);
    saturation(saturatedRight);
    saturatedLeft *= 1 - 1.6 * mix + 0.83 * mix * mix;
    saturatedRight *= 1 - 1.6 * mix + 0.83 * mix * mix;
    // saturatedLeft *= 1. + mix * mix * mix * mix;
    // saturatedRight *= 1. + mix * mix * mix * mix;
    leftOutput = leftOutput * (1. - mix) + saturatedLeft * mix;
    rightOutput = rightOutput * (1. - mix) + saturatedRight * mix;
    hardLimiter(leftOutput, rightOutput);

  } break;
  case 4: {
    // Bogaudio LMTR then stock VCV clip
    softerLimiterLeft.limit = 0.85;
    softerLimiterRight.limit = 0.85;
    hardLimiter(leftOutput, rightOutput);
    softerLimiterLeft.limit = 0.85;
    softerLimiterRight.limit = 0.85;
    hardClipGain = (1. - outputAmplification) * (1. - outputAmplification);
    leftOutput = hardClip(leftOutput);
    rightOutput = hardClip(rightOutput);
    // modifier = (-9.8 / (-40.5 + (40. * outputAmplification))) + 0.758;
    double modifier = 1. + ((1. / (hardClipGain + 0.000000001)) *
                            (1.2 - outputAmplification));
    leftOutput *= modifier;
    rightOutput *= modifier;

  } break;
  case 5: {
    // Stock VCV clip then Bogaudio LMTR
    softerLimiterLeft.limit = 0.85;
    softerLimiterRight.limit = 0.85;
    softerLimiterLeft.limit = 0.85;
    softerLimiterRight.limit = 0.85;
    hardClipGain = (1. - outputAmplification) * (1. - outputAmplification);
    leftOutput = hardClip(leftOutput);
    rightOutput = hardClip(rightOutput);
    // modifier = (-9.8 / (-40.5 + (40. * outputAmplification))) + 0.758;
    double modifier = 1. + ((1. / (hardClipGain + 0.000000001)) *
                            (1.2 - outputAmplification));
    leftOutput *= modifier;
    rightOutput *= modifier;
    hardLimiter(leftOutput, rightOutput);

  } break;
  case 6: {
    softerLimiterLeft.limit = 0.85;
    softerLimiterRight.limit = 0.85;
    // Foldback distortion. Full wave rectifier that folds back on itself
    campestria::foldbackDistortion(leftOutput, 1. - outputAmplification);
    campestria::foldbackDistortion(rightOutput, 1. - outputAmplification);
    hardLimiter(leftOutput, rightOutput);

  } break;
  case 7: {
    softerLimiterLeft.limit = 0.85;
    softerLimiterRight.limit = 0.85;
    saturatedLeft = leftOutput;
    saturatedRight = rightOutput;
    // Output to zero once past threshold. Simulates ripped speaker
    rippedSpeakerLeft(saturatedLeft,
                      (2. + 2. * outputAmplification * outputAmplification -
                       4. * outputAmplification));
    rippedSpeakerRight(saturatedRight,
                       (2. + 2. * outputAmplification * outputAmplification -
                        4. * outputAmplification));
    mix *= 2;
    if (mix > 1.) {
      mix = 1.;
    }
    leftOutput = leftOutput * (1. - mix) + saturatedLeft * mix;
    rightOutput = rightOutput * (1. - mix) + saturatedRight * mix;
    hardLimiter(leftOutput, rightOutput);

  } break;
  case 8: {
    // Same as last but saturation before ripped speaker
    softerLimiterLeft.limit = 0.85;
    softerLimiterRight.limit = 0.85;
    saturatedLeft = leftOutput;
    saturatedRight = rightOutput;
    rippedSpeakerLeft(saturatedLeft,
                      (2. + 2. * outputAmplification * outputAmplification -
                       4. * outputAmplification));
    rippedSpeakerRight(saturatedRight,
                       (2. + 2. * outputAmplification * outputAmplification -
                        4. * outputAmplification));
    saturatedLeft *= 1. + outputAmplification * 15.;
    saturatedRight *= 1. + outputAmplification * 15.;
    saturation(saturatedLeft);
    saturation(saturatedRight);
    saturatedLeft *= 1 - 1.6 * mix + 0.83 * mix * mix;
    saturatedRight *= 1 - 1.6 * mix + 0.83 * mix * mix;
    mix *= 2;
    if (mix > 1.) {
      mix = 1.;
    }
    leftOutput = leftOutput * (1. - mix) + saturatedLeft * mix;
    rightOutput = rightOutput * (1. - mix) + saturatedRight * mix;
    hardLimiter(leftOutput, rightOutput);

  } break;
  case 9: {
    // Same as last but in addition to saturation there is also a hard clipper
    softerLimiterLeft.limit = 0.85;
    softerLimiterRight.limit = 0.85;
    hardClipGain =
        (1. - outputAmplification) * (1. - outputAmplification) + 0.1;
    leftOutput = hardClip(leftOutput);
    rightOutput = hardClip(rightOutput);
    double modifier = 1. + ((1. / (hardClipGain - 0.1 + 0.000000001)) *
                            (1.2 - outputAmplification));
    leftOutput *= modifier;
    rightOutput *= modifier;

    saturatedLeft = leftOutput;
    saturatedRight = rightOutput;

    rippedSpeakerLeft(saturatedLeft,
                      (2. + 2. * outputAmplification * outputAmplification -
                       4. * outputAmplification));
    rippedSpeakerRight(saturatedRight,
                       (2. + 2. * outputAmplification * outputAmplification -
                        4. * outputAmplification));
    saturatedLeft *= 1. + outputAmplification * 15.;
    saturatedRight *= 1. + outputAmplification * 15.;
    saturation(saturatedLeft);
    saturation(saturatedRight);
    saturatedLeft *= 1 - 1.6 * mix + 0.83 * mix * mix;
    saturatedRight *= 1 - 1.6 * mix + 0.83 * mix * mix;

    mix *= 2;
    if (mix > 1.) {
      mix = 1.;
    }
    leftOutput = leftOutput * (1. - mix) + saturatedLeft * mix;
    rightOutput = rightOutput * (1. - mix) + saturatedRight * mix;

    hardLimiter(leftOutput, rightOutput);

  } break;
  case 10: {
    // Last one is Bogaudio LMTR followed by the ripped speaker
    softerLimiterLeft.limit = 0.85;
    softerLimiterRight.limit = 0.85;
    hardLimiter(leftOutput, rightOutput, -30.0f);
    saturatedLeft = leftOutput;
    saturatedRight = rightOutput;
    rippedSpeakerLeft(saturatedLeft,
                      (2. + 2. * outputAmplification * outputAmplification -
                       4. * outputAmplification));
    rippedSpeakerRight(saturatedRight,
                       (2. + 2. * outputAmplification * outputAmplification -
                        4. * outputAmplification));
    mix *= 2;
    if (mix > 1.) {
      mix = 1.;
    }
    leftOutput = leftOutput * (1. - mix) + saturatedLeft * mix;
    rightOutput = rightOutput * (1. - mix) + saturatedRight * mix;
  } break;
  case 11: {
    params.SetGainMode(0);
  } break;
  }
  softLimiter(leftOutput, rightOutput);
}

campestria::PopFilter clearPopFilter;

inline void prepareToClear() {
  if (clear) {
    triggerClear = true;
    clear = false;
  }

  clearPopCancelValue = clearPopFilter.process(!triggerClear);

  if (clearPopCancelValue > 1 - 1e-30) {
    clearPopFilter.value = 1.;
    clearPopCancelValue = 1.;
  }
}

// unsigned int counter = 0;
void AudioCallback(AudioHandle::InputBuffer x, AudioHandle::OutputBuffer out,
                   size_t size) {
  // phase 1: refresh input signals
  hw.ProcessAnalogControls();
  hw.tap.Debounce();
  checkSwitches();
  prepareLeds(samples.leftInput * minus20dBGain,
              samples.rightInput * minus20dBGain, samples.leftOutput,
              samples.rightOutput);

  // phase 2: refresh derived parameters
  ProcessAllParameters();

  // phase 3: process all audio samples for the block
  for (size_t i = 0; i < size; i += 1) {
    reverb.freeze(freeze);

    interpolatingDelayHold();

    prepareGenericLed();

    prepareToClear();

    samples.leftInput = campestria::hardLimit100_(x[0][i]) * 10.;
    samples.rightInput = campestria::hardLimit100_(x[1][i]) * 10.;

    reverb.process(samples.leftInput * minus18dBGain * minus20dBGain *
                       (1.0 + inputAmplification * 7.) * clearPopCancelValue,
                   samples.rightInput * minus18dBGain * minus20dBGain *
                       (1.0 + inputAmplification * 7.) * clearPopCancelValue);

    samples.leftOutput =
        ((samples.leftInput * (1.0f - params.wet) * 0.1) +
         (reverb.getLeftOutput() * params.wet * clearPopCancelValue));
    samples.rightOutput =
        ((samples.rightInput * (1.0f - params.wet) * 0.1) +
         (reverb.getRightOutput() * params.wet * clearPopCancelValue));

    gainControl(samples.leftOutput, samples.rightOutput);

    out[0][i] = samples.leftOutput;
    out[1][i] = samples.rightOutput;
  }
}

void ProcessLEDs() {
  // If the tone knob is not moving and the mode LEDs are not shining, show
  // audio IO levels on LEDs
  if ((gainModeLedCountdown == 0) and (!toneKnobIsMoving)) {
  }

  // allow gain mode countdown to override the LED state
  if (gainModeLedCountdown > 0) {
    const uint32_t gainModeLEDMask = params.GainMode() + 1;

    --gainModeLedCountdown;
    uint32_t intensity = (gainModeLedCountdown != 0);
    uint32_t w = !!(gainModeLEDMask & 8);
    uint32_t x = !!(gainModeLEDMask & 4);
    uint32_t y = !!(gainModeLEDMask & 2);
    uint32_t z = !!(gainModeLEDMask & 1);
    prepareLeds(w * intensity, x * intensity, y * intensity, z * intensity);
  }
  hw.UpdateLeds();
}

uint32_t testValue = 0;
double maxLoad = 0.;

void SetReverbDefaults() {
  reverb.setSampleRate(hw.AudioSampleRate());

  reverb.setTimeScale(1.007500);
  reverb.setPreDelay(0.000000);

  reverb.setInputFilterLowCutoffPitch(10. * params.inputDampLow);
  reverb.setInputFilterHighCutoffPitch(10. - (10. * params.inputDampHigh));
  reverb.enableInputDiffusion(true);
  reverb.setDecay(0.877465);
  reverb.setTankDiffusion(params.diffusion * 0.7);
  reverb.setTankFilterLowCutFrequency(10. * params.reverbDampLow);
  reverb.setTankFilterHighCutFrequency(10. * (1. - params.reverbDampHigh));
  reverb.setTankModSpeed(1.0);
  reverb.setTankModDepth(0.5);
  reverb.setTankModShape(0.5);
}

void LEDEchoLoop(uint32_t value) {
  while (true) {
    for (int i = 0; i < 4; i++) {
      hw.SetLed(i, 0, 0, 0);
    }
    hw.UpdateLeds();
    System::Delay(1000);
    for (int i = 0; i < 4; i++) {
      hw.SetLed(i, 0, 0, 1);
    }
    hw.UpdateLeds();
    System::Delay(2000);

    for (int low_bit = 28; low_bit >= 0; low_bit -= 4) {
      for (int i = 0; i < 4; i++) {
        hw.SetLed(i, 0, 0, 0);
      }
      hw.UpdateLeds();
      System::Delay(1000);

      uint32_t ledFlags = (value >> low_bit) & 0xF;
      if (ledFlags == 0) {
        for (int i = 0; i < 4; i++) {
          hw.SetLed(i, 1, 1, 1);
        }
      } else {
        hw.SetLed(0, (ledFlags & 8) != 0, 0, 0);
        hw.SetLed(1, (ledFlags & 4) != 0, 0, 0);
        hw.SetLed(2, (ledFlags & 2) != 0, 0, 0);
        hw.SetLed(3, (ledFlags & 1) != 0, 0, 0);
      }
      hw.UpdateLeds();
      System::Delay(1000);
    }
  }
}

int main(void) {
  hw.Init(true);

  const int AudioBlockSize = 32;
  hw.SetAudioBlockSize(AudioBlockSize);
  hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_32KHZ);

  bogLimiter.init();

  softerLimiterLeft.Init(hw.AudioSampleRate());
  softerLimiterRight.Init(hw.AudioSampleRate());

  // LEDs indicate we are starting up
  hw.leds[0].Set(1, 0, 0);
  hw.leds[1].Set(1, 0, 0);
  hw.leds[2].Set(1, 0, 0);
  hw.leds[3].Set(1, 0, 0);
  hw.UpdateLeds();

  for (int i = 0; i < 50; i++) {
    for (int j = 0; j < 144000; j++) {
      sdramData[i][j] = 0.;
    }
  }

  // Setup default settings and load saved data
  storage.Init(Settings());

  SetReverbDefaults();

  hw.seed.audio_handle.SetPostGain(1.0);
  hw.seed.audio_handle.SetOutputCompensation(1.0);

  // default slew is 2 milliseconds which for current parameters is 2
  // audio callback blocks (32 samples at 32 khz), which works out to
  // a coefficient of about 0.001 * block size, but we want some
  // controls to be snappier.
  /*hw.knobs[1].SetCoeff(0.01f * static_cast<float>(AudioBlockSize));
  hw.knobs[2].SetCoeff(0.01f * static_cast<float>(AudioBlockSize));
  hw.knobs[4].SetCoeff(0.01f * static_cast<float>(AudioBlockSize));
  hw.knobs[5].SetCoeff(0.01f * static_cast<float>(AudioBlockSize));*/

  // LEDs indicate that we are ready to go
  hw.leds[0].Set(1, 0, 0);
  hw.leds[1].Set(1, 1, 0);
  hw.leds[2].Set(0, 1, 0);
  hw.leds[3].Set(0, 0, 1);
  hw.UpdateLeds();
  System::Delay(500);
  hw.leds[0].Set(0, 0, 0);
  hw.leds[1].Set(0, 0, 0);
  hw.leds[2].Set(0, 0, 0);
  hw.leds[3].Set(0, 0, 0);
  hw.UpdateLeds();

  hw.StartAdc();

  hw.StartAudio(AudioCallback);

  while (1) {
    checkSwitches();

    if (clearPopCancelValue < 1e-30) {
      if (triggerClear) {
        clearPopFilter.value = 0.;
        clearPopCancelValue = 0.;
        reverb.clear();
        triggerClear = false;
      }
    }
    // Clear buffers. Fingers crossed it works.
    // if(triggerClear) {
    //     if(clearPopCancelValue < 0.0001) {
    //         reverb.clear();
    //     }

    // }
  }
}