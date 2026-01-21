#pragma once

namespace campestria {

// Persistent module settings
struct Settings {
  int gainMode = 0;
  // int buttonMode;
  bool operator!=(const Settings &a) {
    return (a.gainMode != gainMode);
    // or (a.buttonMode != buttonMode);
  }
};

}