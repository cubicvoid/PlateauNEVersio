#pragma once
#include "InterpDelay.hpp"

class AllpassFilter {
public:
    AllpassFilter() {
        //clear();
        gain = 0.;
    }

    AllpassFilter(int maxDelay, int initDelay = 0, double gain = 0.) {
        //clear();
        delay = InterpDelay(maxDelay, initDelay);
        this->gain = gain;
    }

    // inline void initializeAllPassFilter(const int &maxDelay, const double &initDelay = 0, const double &gain = 0.) {
    //     clear();
    //     // delay = InterpDelay(maxDelay, initDelay);
    //     delay.initializeDelay(maxDelay, initDelay);
    //     this->gain = gain;
    // }

    #pragma GCC push_options
    #pragma GCC optimize ("Ofast")

    inline double process() {
        _inSum = input + delay.output * gain;
        output = delay.output + _inSum * gain * -1.;
        delay.input = _inSum;
        delay.process();
        return output;
    }

    #pragma GCC pop_options

    void clear() {
        input = 0.;
        output = 0.;
        _inSum = 0.;
        _outSum = 0.;
        delay.clear();
    }

    inline void setGain(const double &newGain) {
        gain = newGain;
    }

    double input;
    double output;
    InterpDelay delay;

private:
    double gain;
    double _inSum;
    double _outSum;
};