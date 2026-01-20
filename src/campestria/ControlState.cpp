#include "ControlState.hpp"

namespace campestria {

void Switch::Debounce() {
  // update no faster than 1kHz
  uint32_t now = daisy::System::GetNow();
  updated_ = false;

  if (now - last_update_ >= 1) {
    last_update_ = now;
    updated_ = true;

    // shift over, and introduce new state.
    state_ = (state_ << 1) | raw_->RawState();
    if (last_edge_ == EdgeType::RISING) {
      if (state_ == 0x00) {
        last_edge_ = EdgeType::FALLING;
        fall_time_ = now;
      }
    } else {
      if (state_ == 0xff) {
        last_edge_ = EdgeType::RISING;
        rise_time_ = now;
      }
    }
  }
}
} // namespace campestria
