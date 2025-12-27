#pragma once
#include "../../utilities/Utilities.hpp"
#include <cstdint>
#include <vector>

extern float DSY_SDRAM_BSS sdramData[50][144000];
extern unsigned int count;
extern double hold;
extern bool triggerClear;
extern double clearPopCancelValue;

class InterpDelay {
public:
  double input = 0.;
  double output = 0.;
  int bufferNumber = 0;
  int r = 0;
  int upperR = 0;
  int j = 0;
  double dataR = 0.;
  double dataUpperR = 0.;

  // InterpDelay () {}

  InterpDelay(unsigned int maxLength = 512, double initDelayTime = 0.) {
    l = maxLength;
    lDouble = static_cast<double>(maxLength);

    bufferNumber = ++count;

    setDelayTime(initDelayTime);
  }

  // inline void initializeDelay(const unsigned int &length = 512, const double
  // &delayTime = 0.) {
  //     l = length;
  //     lDouble = static_cast<double>(length);

  //     bufferNumber = ++count;

  //     setDelayTime(delayTime);
  // }

#pragma GCC push_options
#pragma GCC optimize("Ofast")

  inline void process() {
    // if(!triggerClear) {
    sdramData[bufferNumber][w] = input;
    r = w - t;

    if (r < 0) {
      r += l;
    }

    ++w;
    if (w >= l) {
      w = 0;
    }

    upperR = r - 1;
    if (upperR < 0) {
      upperR += l;
    }

    dataR = sdramData[bufferNumber][r];
    dataUpperR = sdramData[bufferNumber][upperR];

    dataR *= clearPopCancelValue;
    dataUpperR *= clearPopCancelValue;

    output = hold * (dataR + f * (dataUpperR - dataR));
    //}
  }

#pragma GCC pop_options
#pragma GCC push_options
#pragma GCC optimize("Ofast")

  inline double tap(const int &i) {
    j = w - i;
    if (j < 0) {
      j += l;
    }
    return sdramData[bufferNumber][j];
  }

#pragma GCC pop_options
#pragma GCC push_options
#pragma GCC optimize("Ofast")

  inline void setDelayTime(double newDelayTime) {
    if (newDelayTime >= lDouble) {
      newDelayTime = lDouble - 1.;
    }
    if (newDelayTime < 0.) {
      newDelayTime = 0.;
    }
    t = static_cast<int>(newDelayTime);
    f = newDelayTime - static_cast<double>(t);
  }

#pragma GCC pop_options

  void clear() {
    // uint32_t **tempPtr = (uint32_t**)sdramData;
    for (int i = 0; i < l; ++i) {
      sdramData[bufferNumber][i] = uint32_t(0);
    }
    input = 0.;
    output = 0.;
  }

private:
  int w = 0;
  int t = 0;
  double f = 0.;
  int l = 512;
  double lDouble = 512.;
};