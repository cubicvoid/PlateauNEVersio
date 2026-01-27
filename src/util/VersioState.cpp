#include "VersioState.hpp"

namespace util {
namespace daisy {

using ::daisy::DaisyVersio;

void VersioState::Process(const DaisyVersio &hw) {
  for (int i = 0; i < DaisyVersio::KNOB_LAST; i++) {
    knobs[i].Process(hw.GetKnobValue(i));
  }
  tap.Process(hw.tap);
  topSwitchPos = static_cast<Switch3Pos>(hw.sw[0].Read());
  bottomSwitchPos = static_cast<Switch3Pos>(hw.sw[1].Read());
}

} // namespace daisy
} // namespace util
