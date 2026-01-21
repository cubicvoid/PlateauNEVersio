#include "campestria/Campestria.hpp"

using daisy::DaisyVersio;

namespace campestria {

DaisyVersio hw;
ControlState controlState;

daisy::PersistentStorage<Settings> storage(hw.seed.qspi);
Parameters params(storage);

LEDState ledState;
ButtonState buttonState;
State state;

GainState gainState;

Dattorro reverb(32000, 16, 4.0);

class ToneKnobState {
public:
  ToneKnobState() {
    _applyValue(INPUT_DAMP_LOW, 0);
    _applyValue(INPUT_DAMP_HIGH, 0);
    _applyValue(REVERB_DAMP_LOW, 0);
    _applyValue(REVERB_DAMP_HIGH, 0);
    _applyValue(DIFFUSION, 1);
    _applyValue(INPUT_AMPLIFICATION, 0);
    _applyValue(OUTPUT_AMPLIFICATION, 0);
  }

  void Process() {
    Mode mode = _getMode();
    if (mode_ != mode) {
      ledState.CancelSource(LEDState::Source::TONE_KNOB);
      mode_ = mode;
    }
    if (mode < 0 || mode >= LAST) {
      return;
    }

    const double knobValue = controlState.Knob(Knob::TONE).value;
    if (fabs(knobValue - values[mode]) < 0.01) {
      _applyValue(mode, knobValue);
      if (controlState.Knob(Knob::TONE).moving) {
        ledState.SetSource(LEDState::Source::TONE_KNOB);
      }
    }
  }

  void ApplyLEDs(float led[4]) {
    static constexpr int ledMasks[MODE_COUNT] = {0b1000, 0b0100, 0b0010, 0b0001,
                                                 0b1111, 0b1111, 0b1111};
    if (ledState.GetSource() == LEDState::Source::TONE_KNOB) {
      const uint8_t mask = ledMasks[mode_];
      const double value = values[mode_];

      for (int i = 0; i < 4; i++) {
        led[i] = !!(mask & (8 >> i)) * value;
      }
    }
  }

private:
  enum Mode {
    INPUT_DAMP_LOW,
    INPUT_DAMP_HIGH,
    REVERB_DAMP_LOW,
    REVERB_DAMP_HIGH,
    DIFFUSION,
    INPUT_AMPLIFICATION,
    OUTPUT_AMPLIFICATION,
    LAST
  };
  static constexpr int MODE_COUNT = Mode::LAST;
  double values[MODE_COUNT];
  Mode mode_;

  float leds[4];

  void _applyValue(Mode mode, double knobValue) {
    values[mode] = knobValue;
    switch (mode) {
    case DIFFUSION:
      params.diffusion = knobValue * 0.7;
      break;
    case INPUT_AMPLIFICATION:
      params.inputAmplification = (1.0 + knobValue * 7.) / 8.0;
      break;
    case INPUT_DAMP_HIGH:
      params.inputDampHigh = 10.0 * (1.0 - knobValue);
      break;
    case REVERB_DAMP_HIGH:
      params.reverbDampHigh = 10.0 * (1.0 - knobValue);
      break;
    case INPUT_DAMP_LOW:
      params.inputDampLow = 10.0 * knobValue;
      break;
    case REVERB_DAMP_LOW:
      params.reverbDampLow = 10.0 * knobValue;
      break;
    case OUTPUT_AMPLIFICATION:
      params.outputAmplification = 1.0 - knobValue;
      break;
    default:
      break;
    }
  }

  Mode _getMode() {
    switch (controlState.topSwitch) {
    case SwitchState::LEFT:
      switch (controlState.bottomSwitch) {
      case SwitchState::LEFT:
        return INPUT_DAMP_LOW;
      case SwitchState::CENTER:
        return INPUT_AMPLIFICATION;
      case SwitchState::RIGHT:
        return REVERB_DAMP_LOW;
      }
    case SwitchState::CENTER:
      switch (controlState.bottomSwitch) {
      case SwitchState::LEFT:
        return LAST;
      case SwitchState::CENTER:
        return DIFFUSION;
      case SwitchState::RIGHT:
        return LAST;
      }
    case SwitchState::RIGHT:
      switch (controlState.bottomSwitch) {
      case SwitchState::LEFT:
        return INPUT_DAMP_HIGH;
      case SwitchState::CENTER:
        return OUTPUT_AMPLIFICATION;
      case SwitchState::RIGHT:
        return REVERB_DAMP_HIGH;
      }
    }
    return LAST;
  }
};

ToneKnobState toneKnob;

inline void updatePopFilter() {
  static PopFilter popFilter;
  _InterpDelayPopFilterCoeff = popFilter.process(!state.bufferClearPending);

  if (_InterpDelayPopFilterCoeff > 1 - 1e-30) {
    popFilter.value = 1.;
    _InterpDelayPopFilterCoeff = 1.;
  }
  if (_InterpDelayPopFilterCoeff < 1e-30) {
    if (state.bufferClearPending) {
      popFilter.value = 0.;
      _InterpDelayPopFilterCoeff = 0.;
      reverb.clear();
      state.bufferClearPending = false;
    }
  }
}

// Button has three modes, gain control, buffer clear, freeze.
// Hold down the button for 10 seconds, when the LEDs come on
// press again within one second to confirm mode change. If x gain control
// mode not confirming will lock mod depth to 3.125%. In buffer clear
// mode, a rising edge will trigger the buffers to clear. In freeze mode,
// holding the button will freeze the buffers.
void ButtonState::Process() {
  if (controlState.tap.RisingEdge()) {
    if (state.awaitingConfirmation &&
        ledState.GetSource() == LEDState::Source::BUTTON_CONFIRM) {
      // Confirmed, advance to next button mode.
      NextMode();
      state.awaitingConfirmation = false;
      ledState.SetSource(LEDState::Source::NONE);
    } else if (GetMode() == Mode::CLEAR) {
      // In clear mode when we aren't waiting on confirmation, a rising
      // edge means clear the reverb buffers.
      state.bufferClearPending = true;
    }
  } else if (controlState.tap.FallingEdge()) {
    const float pressTime = controlState.tap.SecondsSinceLastPress();
    if (10.0 < pressTime && pressTime < 11.0) {
      // Button released between seconds 10 and 11, enable confirmation
      // sequence.
      state.awaitingConfirmation = true;
    }
    if (GetMode() == Mode::GAIN) {
      if (pressTime < 0.25) {
        // Released after < 1/4 second, move to next gain mode
        params.SetGainMode(params.GainMode() + 1);
        ledState.SetSource(LEDState::Source::GAIN_MODE);
      }
    }
  } else if (controlState.tap.Pressed()) {
    const float pressTime = controlState.tap.SecondsSinceLastPress();
    if (!state.awaitingConfirmation && 10 <= pressTime && pressTime < 11) {
      // We hit 10 seconds, start the LED timer to signal the confirmation
      // sequence.
      state.awaitingConfirmation = true;
      ledState.SetSource(LEDState::Source::BUTTON_CONFIRM);
    }
    if (GetMode() == Mode::GAIN) {
      if (pressTime < 5) {
        // Have been holding for less than five seconds, show current
        // gain mode on LEDs.
        ledState.SetSource(LEDState::Source::GAIN_MODE);
      }

      if (state.awaitingConfirmation && pressTime > 11) {
        // 11 second button hold in gain mode with no confirmation sequence,
        // toggle locked mod depth mode
        state.awaitingConfirmation = false;
        state.lockModDepthTo3_125_ = !state.lockModDepthTo3_125_;
        if (state.lockModDepthTo3_125_) {
          // contract mod depth slightly towards center
          state.lockedModDepthValue = 0.5 + 0.9375 * params.modDepth;
          state.lockModDepthTo3_125_ = true;
        }
      }
    }
  }
}

inline void interpolatingDelayHold() {
  if (daisy::System::GetNow() - state.startTime >= 6000) {
    _InterpDelayHold = 1.;
  }
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
    params.modDepth = state.lockedModDepthValue;
    params.modShape = 0.001 + (knobValue * 0.998);
  } else {
    params.modDepth = knobValue * 16.;
    params.modShape = 0.5;
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

void ApplyParameters() {
  ledState.Process();
  buttonState.Process();

  gainState.Apply(params);

  interpolatingDelayHold();

  reverb.setTimeScale(params.timeScale);
  reverb.setPreDelay(params.preDelay);
  reverb.setDecay(params.decay);
  reverb.setTankModSpeed(params.modSpeed);
  reverb.setTankModShape(params.modShape);
  reverb.setTankModDepth(params.modDepth);
  reverb.setInputFilterHighCutoffPitch(10. - (10. * params.inputDampHigh));
  reverb.setTankFilterHighCutFrequency(10. - (10. * params.reverbDampHigh));
  reverb.setInputFilterLowCutoffPitch(params.inputDampLow * 10.);
  reverb.setTankFilterLowCutFrequency(params.reverbDampLow * 10.);
  reverb.setTankDiffusion(params.diffusion);
  reverb.enableInputDiffusion(params.diffusion != 0);
  reverb.freeze(params.freeze);
}

void RefreshParameters() {
  ProcessTimeScale();
  ProcessMix();
  ProcessModSpeed();
  ProcessModDepth();
  ProcessPreDelay();
  toneKnob.Process();
}

inline void GainMode0(double *left, double *right) {}

void LEDState::Apply() {
  float led[4];

  switch (ledState.GetSource()) {
  case LEDState::Source::BUTTON_CONFIRM:
  case LEDState::Source::BUFFER_CLEAR:
    led[0] = led[1] = led[2] = led[3] = 1.0f;
    break;
  case LEDState::Source::GAIN_MODE: {
    const uint32_t gainModeLEDMask = params.GainMode() + 1;
    led[0] = !!(gainModeLEDMask & 8);
    led[1] = !!(gainModeLEDMask & 4);
    led[2] = !!(gainModeLEDMask & 2);
    led[3] = !!(gainModeLEDMask & 1);

  } break;
  case LEDState::Source::TONE_KNOB:
    toneKnob.ApplyLEDs(led);
    break;
  default:
    led[0] = state.rmsLeftInput;
    led[1] = state.rmsRightInput;
    led[2] = state.rmsLeftOutput;
    led[3] = state.rmsRightInput;
  }

  for (int i = 0; i < 4; i++) {
    hw.SetLed(i, led[i], 0.0f, 0.0f);
  }

  hw.UpdateLeds();
}

// unsigned int counter = 0;
void AudioCallback(daisy::AudioHandle::InputBuffer x,
                   daisy::AudioHandle::OutputBuffer out, size_t size) {
  // phase 1: refresh input signals
  controlState.Refresh();

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
    double leftSample = campestria::hardLimit100_(leftInputRaw);
    double rightSample = campestria::hardLimit100_(rightInputRaw);

    updatePopFilter();

    reverb.process(
        leftSample * params.inputAmplification * _InterpDelayPopFilterCoeff,
        rightSample * params.inputAmplification * _InterpDelayPopFilterCoeff);

    double leftOutput =
        ((leftSample * (1.0f - params.wet)) +
         (reverb.getLeftOutput() * params.wet * _InterpDelayPopFilterCoeff));
    double rightOutput =
        ((rightSample * (1.0f - params.wet)) +
         (reverb.getRightOutput() * params.wet * _InterpDelayPopFilterCoeff));

    gainState.gainControl(&leftOutput, &rightOutput);

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
  ledState.Apply();
}

} // namespace campestria

using namespace campestria;

int main(void) {
  hw.Init(true);

  const int AudioBlockSize = 32;
  hw.SetAudioBlockSize(AudioBlockSize);
  hw.SetAudioSampleRate(daisy::SaiHandle::Config::SampleRate::SAI_32KHZ);

  gainState.Init();

  reverb.setSampleRate(hw.AudioSampleRate());

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

  hw.seed.audio_handle.SetPostGain(1.0);
  hw.seed.audio_handle.SetOutputCompensation(1.0);

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
  }
}