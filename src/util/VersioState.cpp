#include "VersioState.hpp"

namespace util {
namespace daisy {

using ::daisy::DaisyVersio;

void VersioState::Refresh(const DaisyVersio &hw) {
  for (int i = 0; i < DaisyVersio::KNOB_LAST; i++) {
    knobs[i].Refresh(hw.GetKnobValue(i));
  }
  tap.Debounce(hw.tap);
  topSwitch.Refresh(hw.sw[0]);
  bottomSwitch.Refresh(hw.sw[1]);
}

} // namespace daisy
} // namespace util
