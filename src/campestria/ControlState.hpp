#pragma once

#include "../util/VersioState.hpp"
#include "Core.hpp"

#include <algorithm>

namespace campestria {

using daisy::DaisyVersio;
using namespace util::daisy;

enum class KnobID {
  WET,
  MOD_SPEED,
  TONE,
  MOD_DEPTH,
  DECAY,
  TIME_SCALE,
  PRE_DELAY,
  LAST
};

enum class ToneKnobMode {
  INPUT_DAMP_LOW,
  INPUT_DAMP_HIGH,
  REVERB_DAMP_LOW,
  REVERB_DAMP_HIGH,
  DIFFUSION,
  INPUT_AMPLIFICATION,
  OUTPUT_AMPLIFICATION,
  LAST
};
static constexpr int TONE_KNOB_MODE_COUNT =
    static_cast<int>(ToneKnobMode::LAST);

static constexpr int TONE_KNOB_LED_MASKS[TONE_KNOB_MODE_COUNT] = {
    0b1000, 0b0100, 0b0010, 0b0001, 0b1111, 0b1111, 0b1111};

inline uint8_t ToneKnobLEDMask(ToneKnobMode mode) {
  int index = static_cast<int>(mode);
  if (index < 0 || index >= TONE_KNOB_MODE_COUNT) {
    return 0;
  }
  return TONE_KNOB_LED_MASKS[index];
}

class ToneKnobState {
public:
  /*ToneKnobState() {
    _applyValue(ToneKnobMode::INPUT_DAMP_LOW, 0);
    _applyValue(ToneKnobMode::INPUT_DAMP_HIGH, 0);
    _applyValue(ToneKnobMode::REVERB_DAMP_LOW, 0);
    _applyValue(ToneKnobMode::REVERB_DAMP_HIGH, 0);
    _applyValue(ToneKnobMode::DIFFUSION, 1);
    _applyValue(ToneKnobMode::INPUT_AMPLIFICATION, 0);
    _applyValue(ToneKnobMode::OUTPUT_AMPLIFICATION, 0);
  }*/

  void Process(Switch3Pos topSwitch, Switch3Pos bottomSwitch,
               const SmoothedKnob &toneKnob);

  /*void ApplyLEDs(float led[4]) {
    if (ledState.GetSource() == LEDState::Source::TONE_KNOB) {
      const uint8_t mask = ledMasks[mode];
      const double value = values[mode];

      for (int i = 0; i < 4; i++) {
        led[i] = !!(mask & (8 >> i)) * value;
      }
    }
  }*/

  bool Updated() { return updated; }

private:
  bool updated;
  double values[TONE_KNOB_MODE_COUNT];
  ToneKnobMode mode;

  float leds[4];

  /*void _applyValue(double knobValue) {
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
  }*/
};

class LEDState {
public:
  enum class Source {
    NONE,
    BUTTON_CONFIRM,
    BUFFER_CLEAR,
    GAIN_MODE,
    TONE_KNOB
  };

  void SetSource(Source source, float timeoutSec = 1.0f) {
    source_ = source;
    if (source != Source::NONE) {
      ledTimer_.Start(timeoutSec);
    } else {
      ledTimer_.Cancel();
    }
  }

  void CancelSource(Source source) {
    if (source_ == source) {
      ledTimer_.Cancel();
      source = Source::NONE;
    }
  }

  Source GetSource() { return source_; }

  void Process() {
    ledTimer_.Process();
    if (!ledTimer_.Active()) {
      source_ = Source::NONE;
    }
  }

  void Apply();

private:
  Source source_ = Source::NONE;
  CallbackRateTimer ledTimer_;
};

enum class ButtonMode { GAIN, CLEAR, FREEZE };

class ButtonState {
public:
  ButtonMode GetMode() { return mode_; }
  void NextMode() { mode_ = _nextMode(mode_); }

  void Process();

private:
  ButtonMode mode_ = ButtonMode::GAIN;

  ButtonMode _nextMode(ButtonMode mode) {
    switch (mode) {
    case ButtonMode::GAIN:
      return ButtonMode::CLEAR;
    case ButtonMode::CLEAR:
      return ButtonMode::FREEZE;
    default:
      return ButtonMode::GAIN;
    }
  }
};

// The logical state of the input parameters from the DaisyVersio driver after
// basic clipping and smoothing.
class ControlState {
public:
  // Initialize specifying how often the Refresh method will be called
  // per second.
  void Init(const DaisyVersio &hw, uint32_t refreshFreq) {
    versio.Init(hw, refreshFreq);
  }

  const SmoothedKnob &Knob(KnobID knob) const {
    return versio.Knob(static_cast<DaisyVersio::AV_KNOBS>(knob));
  }

  void Refresh(const daisy::DaisyVersio &hw);

  TriggerInput bufferClearTrigger;

private:
  VersioState versio;
  // SmoothedKnob knobs[DaisyVersio::KNOB_LAST];
  // Switch tap;

  ToneKnobState toneKnob;
  ButtonState buttonState;
  LEDState ledState;

  bool awaitingConfirmation_;
  double lockedModDepthValue = 0.;
  bool lockModDepthTo3_125_ = false;

  void _refreshTap();
};

} // namespace campestria
