#pragma once

#include "Core.hpp"

namespace campestria {
namespace debug {

void LEDEchoLoop(uint32_t value) {
  while (true) {
    for (int i = 0; i < 4; i++) {
      hw.SetLed(i, 0, 0, 0);
    }
    hw.UpdateLeds();
    daisy::System::Delay(1000);
    for (int i = 0; i < 4; i++) {
      hw.SetLed(i, 0, 0, 1);
    }
    hw.UpdateLeds();
    daisy::System::Delay(2000);

    for (int low_bit = 28; low_bit >= 0; low_bit -= 4) {
      for (int i = 0; i < 4; i++) {
        hw.SetLed(i, 0, 0, 0);
      }
      hw.UpdateLeds();
      daisy::System::Delay(1000);

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
      daisy::System::Delay(1000);
    }
  }
}

} // namespace debug

} // namespace campestria
