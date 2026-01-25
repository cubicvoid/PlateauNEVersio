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

ReverbState reverbState;

// ToneKnobState toneKnob;

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
      reverbState.Clear();
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
/*void ButtonState::Process() {
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
}*/

inline void interpolatingDelayHold() {
  if (daisy::System::GetNow() - state.startTime >= 6000) {
    _InterpDelayHold = 1.;
  }
}

inline float SnappedToUnitInterval(float v) {
  return (v < 0.01f) ? 0.0f : (v > 0.99) ? 1.0f : (v - 0.01f) / 0.98f;
}

// void ProcessTimeScale() {
//   params.timeScale = controlState.Knob(Knob::TIME_SCALE).value;
// }

// void ProcessMix() { params.wet = controlState.Knob(Knob::WET).value; }

// void ProcessModSpeed() {
//   // Unlike mix, mod speed need not be locked to zero. Mod speed is not
//   // succeptible to noise
//   params.modSpeed = 0.5 + (controlState.Knob(Knob::MOD_SPEED).value * 100.);
// //

void ProcessModDepth() {}

// inline void ProcessDecay() {
//   float scaledKnob =
//       0.0001 + 0.7999 * (1 - controlState.Knob(Knob::DECAY).value);
//   params.decay = 1 - (scaledKnob * scaledKnob);
// }
// void ProcessPreDelay() {
//   params.preDelay = controlState.Knob(Knob::PRE_DELAY).value;
// }

void ApplyParameters() {
  ledState.Process();
  buttonState.Process();

  gainState.Apply(params);

  interpolatingDelayHold();

  reverbState.Apply(params);
}

void RefreshParameters() {
  // ProcessTimeScale();
  // ProcessMix();
  // ProcessModSpeed();
  // ProcessDecay();
  ProcessModDepth();
  // ProcessPreDelay();
  // toneKnob.Process();
}

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
  // phase 0: update raw hardware state
  hw.ProcessAllControls();

  // phase 1: refresh processed control inputs from raw state
  controlState.Refresh(hw);

  // phase 2: refresh derived parameters
  params.Apply(controlState);
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

    reverbState.Process(
        leftSample * params.inputAmplification * _InterpDelayPopFilterCoeff,
        rightSample * params.inputAmplification * _InterpDelayPopFilterCoeff);

    double leftOutput = ((leftSample * (1.0f - params.wet)) +
                         (reverbState.GetLeftOutput() * params.wet *
                          _InterpDelayPopFilterCoeff));
    double rightOutput = ((rightSample * (1.0f - params.wet)) +
                          (reverbState.GetRightOutput() * params.wet *
                           _InterpDelayPopFilterCoeff));

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

void ShowStartupLEDs() {
  // LEDs indicate we are starting up
  hw.leds[0].Set(1, 0, 0);
  hw.leds[1].Set(1, 0, 0);
  hw.leds[2].Set(1, 0, 0);
  hw.leds[3].Set(1, 0, 0);
  hw.UpdateLeds();
}

void ShowReadyLEDs() {
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
}

void Init() {}

int main(void) {
  hw.Init(true);

  const int AudioBlockSize = 32;
  hw.SetAudioBlockSize(AudioBlockSize);
  hw.SetAudioSampleRate(daisy::SaiHandle::Config::SampleRate::SAI_32KHZ);

  ShowStartupLEDs();

  // Load / initialize persistent settings
  storage.Init(Settings());

  gainState.Init();
  reverbState.Init(hw.AudioSampleRate());

  ShowReadyLEDs();

  hw.StartAdc();

  controlState.Init(hw.AudioCallbackRate());

  hw.StartAudio(AudioCallback);

  while (1) {
  }
}