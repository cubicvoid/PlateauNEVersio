#pragma once

#include "Core.hpp"

namespace campestria {

// Persistent module settings
class Settings {
public:
  Settings(daisy::QSPIHandle qspi) : storage(qspi) { storage.Init(Data()); }

  void Load(Parameters *params) {
    params->SetGainMode(storage.GetSettings().gainMode);
  }

  void Process(const Parameters &params) {
    storage.GetSettings().gainMode = params.GainMode();
    storage.Save();
  }

private:
  class Data {
  public:
    int gainMode = 0;
    // int buttonMode;
    bool operator!=(const Data &a) {
      return (a.gainMode != gainMode);
      // or (a.buttonMode != buttonMode);
    }
  };

  ::daisy::PersistentStorage<Data> storage;
};

} // namespace campestria