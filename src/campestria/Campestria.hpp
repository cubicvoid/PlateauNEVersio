#pragma once

#include "ControlState.hpp"
#include "Core.hpp"
#include "Distortion.hpp"
#include "GainState.hpp"
#include "Parameters.hpp"
#include "Settings.hpp"
#include "Utility.hpp"

#include <algorithm>

namespace campestria {

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

class ButtonState {
public:
  enum class Mode { GAIN, CLEAR, FREEZE };

  Mode GetMode() { return mode_; }
  void NextMode() { mode_ = _nextMode(mode_); }

  void Process();

private:
  Mode mode_ = Mode::GAIN;

  Mode _nextMode(Mode mode) {
    switch (mode) {
    case Mode::GAIN:
      return Mode::CLEAR;
    case Mode::CLEAR:
      return Mode::FREEZE;
    default:
      return Mode::GAIN;
    }
  }
};

class State {
public:
  uint32_t startTime = daisy::System::GetNow();

  bool awaitingConfirmation = false;

  double lockedModDepthValue = 0.;
  bool lockModDepthTo3_125_ = false;

  bool bufferClearPending = false;

  float rmsLeftInput = 0.0f;
  float rmsRightInput = 0.0f;

  float rmsLeftOutput = 0.0f;
  float rmsRightOutput = 0.0f;
};

} // namespace campestria
