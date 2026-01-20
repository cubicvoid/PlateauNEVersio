#include "campestria/Campestria.hpp"

// using namespace daisy;
// using namespace daisysp;
//  using namespace bogaudio;

namespace campestria {

enum class ButtonMode { Gain, Clear, Freeze };

using daisy::DaisyVersio;

DaisyVersio hw;

using namespace campestria;

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
daisy::PersistentStorage<Settings> storage(hw.seed.qspi);

LimiterAttackHoldRelease softerLimiterLeft;
LimiterAttackHoldRelease softerLimiterRight;

bogaudio::Lmtr bogLimiter;

const double minus18dBGain = 0.12589254;
const double minus20dBGain = 0.1;

double outputAmplification = 0.0;
double inputAmplification = 0.0;

struct Parameters {
  double wet = 0.5;

  double decay = 0.877465;

  double modDepth = 0.;
  double modShape = 0.;

  double preDelay = 0.;

  double timeScale = 1.0;

  double modSpeed = 0.0;

  double diffusion = 1.;

  double inputDampLow = 0.;
  double inputDampHigh = 0.;

  double reverbDampLow = 0.;
  double reverbDampHigh = 0.;

  uint32_t GainMode() { return storage.GetSettings().gainMode; }
  void SetGainMode(uint32_t mode) {
    storage.GetSettings().gainMode = mode;
    storage.Save();
  }
};

Parameters params;

unsigned int gainModeLedCountdown = 0;
const float gainModeLedDisplayTime = 1.0;

inline void prepareLeds(const double &w, const double &x, const double &y,
                        const double &z) {
  hw.SetLed(0, w, 0.0f, 0.0f);
  hw.SetLed(1, x, 0.0f, 0.0f);
  hw.SetLed(2, y, 0.0f, 0.0f);
  hw.SetLed(3, z, 0.0f, 0.0f);
}

struct State {
  static constexpr float TONE_KNOB_HOLD_SEC = 1.0;

  unsigned int buttonHoldTimer = 0;
  unsigned int buttonOffTimer = 0;

  double lockedModDepthValue = 0.;

  bool lockModDepthTo3_125_ = false;

  float rmsLeftInput = 0.0f;
  float rmsRightInput = 0.0f;

  float rmsLeftOutput = 0.0f;
  float rmsRightOutput = 0.0f;

  unsigned int toneKnobLedCountdown = 0;
  float toneKnobLEDs[4];

  ButtonMode buttonMode = ButtonMode::Gain;

  void StartToneKnobLEDCountdown() {
    toneKnobLedCountdown = TONE_KNOB_HOLD_SEC * hw.AudioCallbackRate();
  }

  void AdvanceToneKnobLEDCountdown() {
    if (toneKnobLedCountdown > 0) {
      if (--toneKnobLedCountdown > 0) {
        prepareLeds(toneKnobLEDs[0], toneKnobLEDs[1], toneKnobLEDs[2],
                    toneKnobLEDs[3]);
      } else {
        prepareLeds(0., 0., 0., 0.);
      }
    }
  }

  void SetToneKnobLEDs(float w, float x, float y, float z) {
    toneKnobLEDs[0] = w;
    toneKnobLEDs[1] = x;
    toneKnobLEDs[2] = y;
    toneKnobLEDs[3] = z;
  }

  void NextButtonMode() {
    switch (buttonMode) {
    case ButtonMode::Gain:
      buttonMode = ButtonMode::Clear;
      break;
    case ButtonMode::Clear:
      buttonMode = ButtonMode::Freeze;
      break;
    case ButtonMode::Freeze:
      buttonMode = ButtonMode::Gain;
      break;
    }
  }
};

State state;

double volumeChange = 0.;

unsigned int holdCount = 0;

bool confirmationSequence = false;
bool confirmationSequenceOne = false;
bool confirmationSequenceTwo = false;
unsigned int confirmationSequenceCounter = 0;
unsigned int confirmationSequenceTimer = 0;

Dattorro reverb(32000, 16, 4.0);

bool diffusionEnabled = true;

ControlState controlState;

bool clear = false;
bool triggerClear = false;

struct LedTimer {
  static constexpr float LED_TIMEOUT = 1.0f;

  unsigned int genericLedCountdown = 0;

  void Start() { genericLedCountdown = LED_TIMEOUT * hw.AudioCallbackRate(); }

  void Process() {
    if (genericLedCountdown > 0) {
      if (--genericLedCountdown > 0) {
        prepareLeds(1, 1, 1, 1);
      } else {
        prepareLeds(0, 0, 0, 0);
      }
    }
  }
};

LedTimer ledTimer;

bool freeze = false;

inline void saturation(double x, double *xOut) {
  x = x * (27. + x * x) / (27. + 9. * x * x);
}

inline void saturation(double *x) { saturation(*x, x); }

// Fast hyperbolic tangent function.
inline void hardLimiter(double *x, double *y, float thresholdDb = -24.0f) {
  bogLimiter.engine.thresholdDb = thresholdDb;
  bogLimiter.processChannel(*x, *y, *x, *y);
}

inline void softLimiter(double &x, double &y, double *xOut, double *yOut) {
  *xOut = softerLimiterLeft.sample(x);
  *yOut = softerLimiterRight.sample(y);
}

// const double hardClipGain = 0.85;
inline double hardClip(const double &x, double limit) {
  return std::max(std::min(x, limit), -limit);
}

const unsigned int rippedSpeakerHoldSamples = 32;

struct RippedSpeakerFilter {
  GateFilter leftGateFilter;
  unsigned int rippedCount = 0;
  double value = 1.;
};

inline void rippedSpeakerLeft(double &x, double threshold) {
  static GateFilter leftGateFilter;
  static unsigned int rippedCountLeft = 0;
  static double leftValue = 1.;
  if (rippedCountLeft < rippedSpeakerHoldSamples) {
    ++rippedCountLeft;
    leftValue = 0.;
  } else if (rippedCountLeft == rippedSpeakerHoldSamples) {
    rippedCountLeft = rippedSpeakerHoldSamples + 1;
    leftValue = 1.;
  }
  if (x > threshold || x < -threshold) {
    rippedCountLeft = 0;
    leftValue = 0.;
  }
  x *= leftGateFilter.process(leftValue);
}

inline void rippedSpeakerRight(double &x, double threshold) {
  static GateFilter rightGateFilter;
  static unsigned int rippedCountRight = 0;
  static double rightValue = 1.;
  if (rippedCountRight < rippedSpeakerHoldSamples) {
    ++rippedCountRight;
    rightValue = 0.;
  } else if (rippedCountRight == rippedSpeakerHoldSamples) {
    rippedCountRight = rippedSpeakerHoldSamples + 1;
    rightValue = 1.;
  }
  if (x > threshold || x < -threshold) {
    rippedCountRight = 0;
    rightValue = 0.;
  }
  x *= rightGateFilter.process(rightValue);
}

inline void ReadSwitches() {
  controlState.topSwitch = static_cast<SwitchState>(hw.sw[0].Read());
  controlState.bottomSwitch = static_cast<SwitchState>(hw.sw[1].Read());
}

inline bool SwitchStatesMatch(SwitchState topState, SwitchState bottomState) {
  return controlState.topSwitch == topState &&
         controlState.bottomSwitch == bottomState;
}

inline void ProcessSwitches() {
  if (SwitchStatesMatch(SwitchState::RIGHT, SwitchState::LEFT)) {
    const float knobValue = controlState.Knob(Knob::TONE).value;
    if (fabs(knobValue - params.inputDampHigh) < 0.01) {
      params.inputDampHigh = knobValue;
      reverb.setInputFilterHighCutoffPitch(10. - (10. * params.inputDampHigh));
      if (controlState.Knob(Knob::TONE).moving) {
        state.StartToneKnobLEDCountdown();
      }
      state.SetToneKnobLEDs(0., params.inputDampHigh, 0., 0.);
    }
  } else if (SwitchStatesMatch(SwitchState::RIGHT, SwitchState::RIGHT)) {
    const float knobValue = controlState.Knob(Knob::TONE).value;
    if (fabs(knobValue - params.reverbDampHigh) < 0.01) {
      params.reverbDampHigh = knobValue;
      reverb.setTankFilterHighCutFrequency(10. - (10. * params.reverbDampHigh));
      if (controlState.Knob(Knob::TONE).moving) {
        state.StartToneKnobLEDCountdown();
      }
      state.SetToneKnobLEDs(0., 0., 0., params.reverbDampHigh);
    }
  } else if (SwitchStatesMatch(SwitchState::LEFT, SwitchState::LEFT)) {
    const float knobValue = controlState.Knob(Knob::TONE).value;
    params.inputDampLow = controlState.Knob(Knob::TONE).value;
    if (fabs(knobValue - params.inputDampLow) < 0.01) {
      params.inputDampLow = knobValue;
      reverb.setInputFilterLowCutoffPitch(params.inputDampLow * 10.);
      if (controlState.Knob(Knob::TONE).moving) {
        state.StartToneKnobLEDCountdown();
      }
      state.SetToneKnobLEDs(params.inputDampLow, 0., 0., 0.);
    }
  } else if (SwitchStatesMatch(SwitchState::LEFT, SwitchState::RIGHT)) {
    const float knobValue = controlState.Knob(Knob::TONE).value;
    if (fabs(knobValue - params.reverbDampLow) < 0.01) {
      params.reverbDampLow = knobValue;
      reverb.setTankFilterLowCutFrequency(params.reverbDampLow * 10.);
      if (controlState.Knob(Knob::TONE).moving) {
        state.StartToneKnobLEDCountdown();
      }
      state.SetToneKnobLEDs(0., 0., params.reverbDampLow, 0.);
    }
  } else if (SwitchStatesMatch(SwitchState::CENTER, SwitchState::CENTER)) {
    const float knobValue = controlState.Knob(Knob::TONE).value;
    if (fabs(knobValue - params.diffusion) < 0.01) {
      params.diffusion = knobValue;
      if (params.diffusion == 0.) {
        if (diffusionEnabled) {
          diffusionEnabled = false;
        }
      } else {
        if (!diffusionEnabled) {
          diffusionEnabled = true;
        }
        reverb.setTankDiffusion(params.diffusion * 0.7);
      }
      if (controlState.Knob(Knob::TONE).moving) {
        state.StartToneKnobLEDCountdown();
      }
      state.SetToneKnobLEDs(params.diffusion, params.diffusion,
                            params.diffusion, params.diffusion);
    }
  } else if (SwitchStatesMatch(SwitchState::CENTER, SwitchState::LEFT)) {
    // unused
  } else if (SwitchStatesMatch(SwitchState::CENTER, SwitchState::RIGHT)) {
    // unused
  } else if (SwitchStatesMatch(SwitchState::LEFT, SwitchState::CENTER)) {
    const float knobValue = controlState.Knob(Knob::TONE).value;
    if (fabs(knobValue - inputAmplification) < 0.01) {
      inputAmplification = knobValue;
      if (controlState.Knob(Knob::TONE).moving) {
        state.StartToneKnobLEDCountdown();
      }
      state.SetToneKnobLEDs(inputAmplification, inputAmplification,
                            inputAmplification, inputAmplification);
    }
  } else if (SwitchStatesMatch(SwitchState::RIGHT, SwitchState::CENTER)) {
    const float knobValue = controlState.Knob(Knob::TONE).value;
    if (fabs(knobValue - outputAmplification) < 0.01) {
      outputAmplification = knobValue;
      if (controlState.Knob(Knob::TONE).moving) {
        state.StartToneKnobLEDCountdown();
      }
      state.SetToneKnobLEDs(outputAmplification, outputAmplification,
                            outputAmplification, outputAmplification);
    }
  }
  state.AdvanceToneKnobLEDCountdown();
}

PopFilter clearPopFilter;
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
// mode not confirming will lock mod depth to 3.125%. In buffer clear
// mode, a rising edge will trigger the buffers to clear. In freeze mode,
// holding the button will freeze the buffers.
inline void ProcessButton() {

  if (hw.tap.Pressed()) {
    state.buttonHoldTimer++;
  } else {
    state.buttonHoldTimer = 0;
  }

  switch (state.buttonMode) {
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
    if (state.buttonHoldTimer ==
        10 * static_cast<unsigned int>(hw.AudioCallbackRate())) {
      ledTimer.Start();
    }
    if (state.buttonHoldTimer ==
        11 * static_cast<unsigned int>(hw.AudioCallbackRate())) {
      if (state.buttonMode == ButtonMode::Gain) {

        // params.modDepth = 0.5 + (state.lockedModDepthValue * 15.5);
        if (state.lockModDepthTo3_125_) {

          state.lockModDepthTo3_125_ = false;
          params.modShape = 0.5;
        } else {
          // contract mod depth slightly towards center
          params.modDepth = 0.5 + 0.9375 * params.modDepth;
          state.lockModDepthTo3_125_ = true;
        }
      }
    }
    if (hw.tap.RisingEdge()) {
      if (confirmationSequence) {
        if (ledTimer.genericLedCountdown > 0) {
          state.NextButtonMode();
          confirmationSequence = false;
        } else {
          // This might be redundant.
          confirmationSequence = false;
        }
      }
      if (state.buttonMode == ButtonMode::Clear) {
        ledTimer.Start();
        clear = true;
      }
    }
  } else {

    if (hw.tap.FallingEdge()) {
      if ((state.buttonHoldTimer > 10 * hw.AudioCallbackRate()) and
          (state.buttonHoldTimer < 11 * hw.AudioCallbackRate())) {
        // Button released between seconds 10 and 11, enable confirmation
        // sequence.
        confirmationSequence = true;
      }
      if (state.buttonMode == ButtonMode::Gain) {
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
    _InterpDelayHold = 1.;
}
inline float SnappedToUnitInterval(float v) {
  return (v < 0.01f) ? 0.0f : (v > 0.99) ? 1.0f : (v - 0.01f) / 0.98f;
}

void ProcessTimeScale() {
  params.timeScale = controlState.Knob(Knob::TIME_SCALE).value;
}

void ProcessMix() { params.wet = controlState.Knob(Knob::WET).value; }

void ProcessModSpeed() {
  // Unlike mix, mod speed need not be locked to zero. Mod speed is not
  // succeptible to noise
  params.modSpeed = 0.5 + (controlState.Knob(Knob::MOD_SPEED).value * 100.);
}

void ProcessModDepth() {
  float knobValue = controlState.Knob(Knob::MOD_DEPTH).value;
  // Ability to lock mod depth to the equivalent default 3.125% of VCV
  // rack
  if (state.lockModDepthTo3_125_) {
    params.modShape = 0.001 + (knobValue * 0.998);
  } else {
    params.modDepth = knobValue * 16.;
  }
}

inline void ProcessDecay() {
  float scaledKnob =
      0.0001 + 0.7999 * (1 - controlState.Knob(Knob::DECAY).value);
  params.decay = 1 - (scaledKnob * scaledKnob);
}
void ProcessPreDelay() {
  params.preDelay = controlState.Knob(Knob::PRE_DELAY).value;
}

inline void ApplyParameters() {
  if (clearPopCancelValue < 1e-30) {
    if (triggerClear) {
      clearPopFilter.value = 0.;
      clearPopCancelValue = 0.;
      reverb.clear();
      triggerClear = false;
    }
  }

  interpolatingDelayHold();

  reverb.setTimeScale(params.timeScale);
  reverb.setPreDelay(params.preDelay);
  reverb.setDecay(params.decay);
  reverb.setTankModSpeed(params.modSpeed);
  reverb.setTankModShape(params.modShape);
  reverb.setTankModDepth(params.modDepth);
  reverb.freeze(freeze);
  reverb.enableInputDiffusion(diffusionEnabled);
}

inline void RefreshParameters() {
  ProcessButton();

  ProcessTimeScale();
  ProcessMix();
  ProcessModSpeed();
  ProcessModDepth();
  ProcessPreDelay();
  ProcessSwitches();
}

inline void GainMode0(double *left, double *right) {}

inline void gainControl(double *left, double *right) {

  double saturatedLeft = *left;
  double saturatedRight = *right;
  double mix = 1. - ((1. - outputAmplification) * (1. - outputAmplification) *
                     (1. - outputAmplification));
  switch (params.GainMode()) {
  case 0: {
    // Regular soft limiter. Rarely clips. Lower limit threshold by
    // turning tone knob up output dynamic setting selected.
    softerLimiterLeft.limit = (0.85 - (outputAmplification * 0.85));
    softerLimiterRight.limit = (0.85 - (outputAmplification * 0.85));
    break;
  }
  case 1: {
    // Same clipper as VCV rack. Lower clip threshold by turning tone knob
    // up with output dynamic setting selected.
    softerLimiterLeft.limit = 0.85;
    softerLimiterRight.limit = 0.85;
    const double clipLimit =
        (1. - outputAmplification) * (1. - outputAmplification);
    const double modifier =
        1. + ((1. / (clipLimit + 0.000000001)) * (1.2 - outputAmplification));
    *left = modifier * hardClip(*left, clipLimit);
    *right = modifier * hardClip(*right, clipLimit);
  } break;
  case 2: {
    // Same as last but with saturation
    softerLimiterLeft.limit = 0.85;
    softerLimiterRight.limit = 0.85;
    const double clipLimit =
        (1. - outputAmplification) * (1. - outputAmplification);
    const double modifier =
        1. + ((1. / (clipLimit + 0.000000001)) * (1.2 - outputAmplification));

    double leftClipped = modifier * hardClip(*left, clipLimit);
    double rightClipped = modifier * hardClip(*right, clipLimit);

    saturatedLeft = leftClipped;
    saturatedRight = rightClipped;
    saturatedLeft *= 1. + outputAmplification * 20.;
    saturatedRight *= 1. + outputAmplification * 20.;
    saturation(&saturatedLeft);
    saturation(&saturatedRight);
    saturatedLeft *= 1 - 1.6 * mix + 0.83 * mix * mix;
    saturatedRight *= 1 - 1.6 * mix + 0.83 * mix * mix;

    *left = leftClipped * (1. - mix) + saturatedLeft * mix;
    *right = rightClipped * (1. - mix) + saturatedRight * mix;
    hardLimiter(left, right);

  } break;
  case 3: {
    // Just saturation. Control gain going into saturation with tone knob
    // output dynamic setting
    softerLimiterLeft.limit = 0.85;
    softerLimiterRight.limit = 0.85;
    saturatedLeft = *left;
    saturatedRight = *right;
    saturatedLeft *= 1. + outputAmplification * 20.;
    saturatedRight *= 1. + outputAmplification * 20.;
    saturation(&saturatedLeft);
    saturation(&saturatedRight);
    saturatedLeft *= 1 - 1.6 * mix + 0.83 * mix * mix;
    saturatedRight *= 1 - 1.6 * mix + 0.83 * mix * mix;
    // saturatedLeft *= 1. + mix * mix * mix * mix;
    // saturatedRight *= 1. + mix * mix * mix * mix;
    *left = *left * (1. - mix) + saturatedLeft * mix;
    *right = *right * (1. - mix) + saturatedRight * mix;
    hardLimiter(left, right);

  } break;
  case 4: {
    // Bogaudio LMTR then stock VCV clip
    softerLimiterLeft.limit = 0.85;
    softerLimiterRight.limit = 0.85;
    hardLimiter(left, right);
    softerLimiterLeft.limit = 0.85;
    softerLimiterRight.limit = 0.85;
    const double clipLimit =
        (1. - outputAmplification) * (1. - outputAmplification);
    const double modifier =
        1. + ((1. / (clipLimit + 0.000000001)) * (1.2 - outputAmplification));
    *left = modifier * hardClip(*left, clipLimit);
    *right = modifier * hardClip(*right, clipLimit);
  } break;
  case 5: {
    // Stock VCV clip then Bogaudio LMTR
    softerLimiterLeft.limit = 0.85;
    softerLimiterRight.limit = 0.85;
    const double clipLimit =
        (1. - outputAmplification) * (1. - outputAmplification);
    const double modifier =
        1. + ((1. / (clipLimit + 0.000000001)) * (1.2 - outputAmplification));

    *left = modifier * hardClip(*left, clipLimit);
    *right = modifier * hardClip(*right, clipLimit);
    hardLimiter(left, right);
  } break;
  case 6: {
    softerLimiterLeft.limit = 0.85;
    softerLimiterRight.limit = 0.85;
    // Foldback distortion. Full wave rectifier that folds back on itself
    campestria::foldbackDistortion(*left, 1. - outputAmplification);
    campestria::foldbackDistortion(*right, 1. - outputAmplification);
    hardLimiter(left, right);
  } break;
  case 7: {
    softerLimiterLeft.limit = 0.85;
    softerLimiterRight.limit = 0.85;
    saturatedLeft = *left;
    saturatedRight = *right;
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
    *left = *left * (1. - mix) + saturatedLeft * mix;
    *left = *right * (1. - mix) + saturatedRight * mix;
    hardLimiter(left, right);

  } break;
  case 8: {
    // Same as last but saturation before ripped speaker
    softerLimiterLeft.limit = 0.85;
    softerLimiterRight.limit = 0.85;
    saturatedLeft = *left;
    saturatedRight = *right;
    rippedSpeakerLeft(saturatedLeft,
                      (2. + 2. * outputAmplification * outputAmplification -
                       4. * outputAmplification));
    rippedSpeakerRight(saturatedRight,
                       (2. + 2. * outputAmplification * outputAmplification -
                        4. * outputAmplification));
    saturatedLeft *= 1. + outputAmplification * 15.;
    saturatedRight *= 1. + outputAmplification * 15.;
    saturation(&saturatedLeft);
    saturation(&saturatedRight);
    saturatedLeft *= 1 - 1.6 * mix + 0.83 * mix * mix;
    saturatedRight *= 1 - 1.6 * mix + 0.83 * mix * mix;
    mix *= 2;
    if (mix > 1.) {
      mix = 1.;
    }
    *left = *left * (1. - mix) + saturatedLeft * mix;
    *right = *right * (1. - mix) + saturatedRight * mix;
    hardLimiter(left, right);

  } break;
  case 9: {
    // Same as last but in addition to saturation there is also a hard
    // clipper
    softerLimiterLeft.limit = 0.85;
    softerLimiterRight.limit = 0.85;
    const double clipLimit =
        (1. - outputAmplification) * (1. - outputAmplification) + 0.1;
    const double modifier = 1. + ((1. / (clipLimit - 0.1 + 0.000000001)) *
                                  (1.2 - outputAmplification));
    *left = modifier * hardClip(*left, clipLimit);
    *right = modifier * hardClip(*right, clipLimit);

    saturatedLeft = *left;
    saturatedRight = *right;

    rippedSpeakerLeft(saturatedLeft,
                      (2. + 2. * outputAmplification * outputAmplification -
                       4. * outputAmplification));
    rippedSpeakerRight(saturatedRight,
                       (2. + 2. * outputAmplification * outputAmplification -
                        4. * outputAmplification));
    saturatedLeft *= 1. + outputAmplification * 15.;
    saturatedRight *= 1. + outputAmplification * 15.;
    saturation(&saturatedLeft);
    saturation(&saturatedRight);
    saturatedLeft *= 1 - 1.6 * mix + 0.83 * mix * mix;
    saturatedRight *= 1 - 1.6 * mix + 0.83 * mix * mix;

    mix = std::min(mix * 2, 1.0);

    *left = *left * (1. - mix) + saturatedLeft * mix;
    *right = *right * (1. - mix) + saturatedRight * mix;

    hardLimiter(left, right);

  } break;
  case 10: {
    // Last one is Bogaudio LMTR followed by the ripped speaker
    softerLimiterLeft.limit = 0.85;
    softerLimiterRight.limit = 0.85;
    hardLimiter(left, left, -30.0f);
    saturatedLeft = *left;
    saturatedRight = *left;
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
    *left = *left * (1. - mix) + saturatedLeft * mix;
    *right = *right * (1. - mix) + saturatedRight * mix;
  } break;
  case 11: {
    params.SetGainMode(0);
  } break;
  }

  softLimiter(*left, *right, left, right);
}

void ReadInputs() {
  hw.ProcessAnalogControls();
  hw.tap.Debounce();
  ReadSwitches();
}

void ProcessLEDs() {
  prepareLeds(state.rmsLeftInput, state.rmsRightInput, state.rmsLeftOutput,
              state.rmsRightInput);

  ledTimer.Process();

  // allow gain mode countdown to override the LED state
  if (gainModeLedCountdown > 0) {
    const uint32_t gainModeLEDMask = params.GainMode() + 1;
    if (--gainModeLedCountdown > 0) {
      prepareLeds(!!(gainModeLEDMask & 8), !!(gainModeLEDMask & 4),
                  !!(gainModeLEDMask & 2), !!(gainModeLEDMask & 1));
    } else {
      prepareLeds(0, 0, 0, 0);
    }
  }

  hw.UpdateLeds();
}

// unsigned int counter = 0;
void AudioCallback(daisy::AudioHandle::InputBuffer x,
                   daisy::AudioHandle::OutputBuffer out, size_t size) {
  // phase 1: refresh input signals
  ReadInputs();

  // phase 2: refresh derived parameters
  RefreshParameters();

  // phase 3: apply parameters to state
  ApplyParameters();

  float leftInputsSquared = 0.0f;
  float rightInputsSquared = 0.0f;
  float leftOutputsSquared = 0.0f;
  float rightOutputsSquared = 0.0f;
  // phase 4: process all audio samples for the block
  for (size_t i = 0; i < size; i += 1) {
    const float leftInputRaw = x[0][i];
    const float rightInputRaw = x[1][i];
    leftInputsSquared += leftInputRaw * leftInputRaw;
    rightInputsSquared += rightInputRaw * rightInputRaw;
    // aren't samples already guaranteed to have absolute value at most 1?
    double leftSample = campestria::hardLimit100_(leftInputRaw) * 10.;
    double rightSample = campestria::hardLimit100_(rightInputRaw) * 10.;

    prepareToClear();

    reverb.process(leftSample * minus18dBGain * minus20dBGain *
                       (1.0 + inputAmplification * 7.) * clearPopCancelValue,
                   rightSample * minus18dBGain * minus20dBGain *
                       (1.0 + inputAmplification * 7.) * clearPopCancelValue);

    double leftOutput =
        ((leftSample * (1.0f - params.wet) * 0.1) +
         (reverb.getLeftOutput() * params.wet * clearPopCancelValue));
    double rightOutput =
        ((rightSample * (1.0f - params.wet) * 0.1) +
         (reverb.getRightOutput() * params.wet * clearPopCancelValue));

    gainControl(&leftOutput, &rightOutput);

    out[0][i] = leftOutput;
    out[1][i] = rightOutput;
    leftOutputsSquared += leftOutput * leftOutput;
    rightOutputsSquared += rightOutput * rightOutput;
  }
  state.rmsLeftInput = sqrtf(leftInputsSquared / size);
  state.rmsRightInput = sqrtf(rightInputsSquared / size);
  state.rmsLeftOutput = sqrtf(leftOutputsSquared / size);
  state.rmsRightOutput = sqrtf(rightOutputsSquared / size);

  // phase 5: display LED state for this block
  ProcessLEDs();
}

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

} // namespace campestria

using namespace campestria;

int main(void) {
  hw.Init(true);

  const int AudioBlockSize = 32;
  hw.SetAudioBlockSize(AudioBlockSize);
  hw.SetAudioSampleRate(daisy::SaiHandle::Config::SampleRate::SAI_32KHZ);

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
  daisy::System::Delay(500);
  hw.leds[0].Set(0, 0, 0);
  hw.leds[1].Set(0, 0, 0);
  hw.leds[2].Set(0, 0, 0);
  hw.leds[3].Set(0, 0, 0);
  hw.UpdateLeds();

  hw.StartAdc();

  controlState.Init(hw.AudioCallbackRate());

  hw.StartAudio(AudioCallback);

  while (1) {
    ReadSwitches();

    // Clear buffers. Fingers crossed it works.
    // if(triggerClear) {
    //     if(clearPopCancelValue < 0.0001) {
    //         reverb.clear();
    //     }

    // }
  }
}