#include "ControlState.hpp"

namespace campestria {

namespace {

uint8_t ControlState::_SetToneKnobLEDs(ToneKnobMode mode) {
  static constexpr uint8_t LED_MASKS[TONE_KNOB_MODE_COUNT] = {
      0b1000, 0b0100, 0b0010, 0b0001, 0b1111, 0b1111, 0b1111};

  int index = static_cast<int>(mode);
  if (index < 0 || index >= TONE_KNOB_MODE_COUNT) {
    return 0;
  }
  const uint8_t mask = LED_MASKS[index];
  double value = toneKnobValues[index];

  for (int i = 0; i < 4; i++) {
    led[i] = !!(mask & (8 >> i)) * value;
  }
}

} // namespace

void ControlState::ApplyLEDs() {
  float led[4];

  switch (ledState.GetSource()) {
  case LEDSource::BUTTON_CONFIRM:
  case LEDSource::BUFFER_CLEAR:
    led[0] = led[1] = led[2] = led[3] = 1.0f;
    break;
  case LEDSource::GAIN_MODE: {
    const uint32_t gainModeLEDMask = params.GainMode() + 1;
    led[0] = !!(gainModeLEDMask & 8);
    led[1] = !!(gainModeLEDMask & 4);
    led[2] = !!(gainModeLEDMask & 2);
    led[3] = !!(gainModeLEDMask & 1);

  } break;
  case LEDSource::TONE_KNOB:
    const uint8_t mask = ToneKnobLEDMask(toneKnobMode);
    const double value = toneKnobValues[mode];

    control.toneKnob.ApplyLEDs(led);
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
}

ToneKnobMode _toneKnobModeForSwitchPositions(Switch3Pos topSwitch,
                                             Switch3Pos bottomSwitch) {
  switch (topSwitch) {
  case Switch3Pos::LEFT:
    switch (bottomSwitch) {
    case Switch3Pos::LEFT:
      return ToneKnobMode::INPUT_DAMP_LOW;
    case Switch3Pos::CENTER:
      return ToneKnobMode::INPUT_AMPLIFICATION;
    case Switch3Pos::RIGHT:
      return ToneKnobMode::REVERB_DAMP_LOW;
    }
  case Switch3Pos::CENTER:
    switch (bottomSwitch) {
    case Switch3Pos::LEFT:
      return ToneKnobMode::LAST;
    case Switch3Pos::CENTER:
      return ToneKnobMode::DIFFUSION;
    case Switch3Pos::RIGHT:
      return ToneKnobMode::LAST;
    }
  case Switch3Pos::RIGHT:
    switch (bottomSwitch) {
    case Switch3Pos::LEFT:
      return ToneKnobMode::INPUT_DAMP_HIGH;
    case Switch3Pos::CENTER:
      return ToneKnobMode::OUTPUT_AMPLIFICATION;
    case Switch3Pos::RIGHT:
      return ToneKnobMode::REVERB_DAMP_HIGH;
    }
  }
  return ToneKnobMode::LAST;
}

void ControlState::_processTap() {
  bufferClearTrigger = false;
  if (versio.Tap().RisingEdge()) {
    if (awaitingConfirmation_ &&
        ledState.GetSource() == LEDSource::BUTTON_CONFIRM) {
      // Confirmed, advance to next button mode.
      buttonState.NextMode();
      awaitingConfirmation_ = false;
      ledState.SetSource(LEDSource::NONE);
    } else if (buttonState.GetMode() == Mode::CLEAR) {
      // In clear mode when we aren't waiting on confirmation, a rising
      // edge means clear the reverb buffers.
      bufferClearTrigger.Set();
    }
  } else if (controlState.tap.FallingEdge()) {
    const float pressTime = controlState.tap.SecondsSinceLastPress();
    if (10.0 < pressTime && pressTime < 11.0) {
      // Button released between seconds 10 and 11, enable confirmation
      // sequence.
      state.awaitingConfirmation = true;
    }
    if (buttonState.GetMode() == Mode::GAIN) {
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
      ledState.SetSource(LEDSource::BUTTON_CONFIRM);
    }
    if (GetMode() == Mode::GAIN) {
      if (pressTime < 5) {
        // Have been holding for less than five seconds, show current
        // gain mode on LEDs.
        ledState.SetSource(LEDSource::GAIN_MODE);
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

void ControlState::Process(const daisy::DaisyVersio &hw) {
  bufferClearTrigger.Reset();
  versio.Process(hw);

  toneKnob.Process(versio.TopSwitchPos(), versio.BottomSwitchPos(),
                   Knob(KnobID::TONE));

  if (toneKnob.Updated() && Knob(KnobID::TONE).moving) {
    ledState.SetSource(LEDSource::TONE_KNOB);
  }

  float knobValue = Knob(KnobID::MOD_DEPTH).value;
  // Ability to lock mod depth to the equivalent default 3.125% of VCV
  // rack
  /*if (lockModDepthTo3_125_) {
    params.modDepth = lockedModDepthValue;
    params.modShape = 0.001 + (knobValue * 0.998);
  } else {
    params.modDepth = knobValue * 16.;
    params.modShape = 0.5;
  }*/

  ledTimer_.Process();
  if (!ledTimer_.Active()) {
    source_ = LEDSource::NONE;
  }
}

void ToneKnobState::Process(Switch3Pos topSwitch, Switch3Pos bottomSwitch,
                            const SmoothedKnob &toneKnob) {
  ToneKnobMode newMode =
      _toneKnobModeForSwitchPositions(topSwitch, bottomSwitch);
  if (mode != newMode) {
    ledState.CancelSource(LEDSource::TONE_KNOB);
    mode = newMode;
  }
  if (mode == ToneKnobMode::LAST) {
    return;
  }

  if (fabs(toneKnob.value - values[mode]) < 0.01) {
    values[mode] = toneKnob.value;
    updated = true;
    _applyValue(mode, knobValue);
  } else {
    updated = false;
  }
}

} // namespace campestria
