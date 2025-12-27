#pragma once
#include <cmath>
#include <cstdint>

#ifndef  M_PI
#define M_PI		3.14159265358979323846
#endif

#define _1_FACT_2 0.5
#define _1_FACT_3 0.1666666667
#define _1_FACT_4 0.04166666667
#define _1_FACT_5 0.008333333333
#define _2M_PI 2.0 * M_PI

class OnePoleLPFilter {
public:
    // OnePoleLPFilter(double cutoffFreq = 22049.0, double initSampleRate = 32000.0);
    // OnePoleLPFilter(double cutoffFreq = 14080.0, double initSampleRate = 32000.0);
    OnePoleLPFilter(double cutoffFreq = 22049.0, double initSampleRate = 32000.0) {
        setSampleRate(initSampleRate);
        setCutoffFreq(cutoffFreq);
    }

    #pragma GCC push_options
    #pragma GCC optimize ("Ofast")

    inline double process() {
        _z =  _a * input + _z * _b;
        output = _z;
        return output;
    }

    #pragma GCC pop_options

    void clear() {
        input = 0.0;
        _z = 0.0;
        output = 0.0;
    }

    void setSampleRate(double sampleRate) {
        _sampleRate = sampleRate;
        _1_sampleRate_pi = (1.0 / sampleRate) * -_2M_PI;
        _maxCutoffFreq = sampleRate / 2.0 - 1.0;
        setCutoffFreq(_cutoffFreq);
    }

    #pragma GCC push_options
    #pragma GCC optimize ("Ofast")

    // inline void setCutoffFreq(const double &cutoffFreq) {
    //     // if (cutoffFreq == _cutoffFreq) {
    //     //     return;
    //     // }

    //     _cutoffFreq = cutoffFreq;
    //     // _b = expf(-_2M_PI * _cutoffFreq * _1_sampleRate);
    //     // Using a linear equation instead of exponential because
    //     // it uses less operations. Not sure if it will make any
    //     // difference. This holds for the expected {-1 < x < 0}
    //     // _b = (0.361 * _cutoffFreq) + 1.;
    //     _b = _cutoffFreq + 1.;
    //     // _b = (0.361 * -_cutoffFreq * _1_sampleRate_pi) + 1;
    //     _a = 1.0 - _b;
    // }

    inline void setCutoffFreq(double cutoffFreq) {
        if (cutoffFreq == _cutoffFreq) {
            return;
        }

        _cutoffFreq = cutoffFreq;
        _b = expf(_cutoffFreq * _1_sampleRate_pi);
        _a = 1.0 - _b;
    }

    #pragma GCC pop_options

    double input = 0.0;
    double output = 0.0;
private:
    double _sampleRate = 32000.0;
    double _1_sampleRate_pi = (1.0 / _sampleRate) * -_2M_PI;
    double _cutoffFreq = 0.0;
    double _maxCutoffFreq = _sampleRate / 2.0;
    double _a = 0.0;
    double _b = 0.0;
    double _z = 0.0;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

class OnePoleHPFilter {
public:
    // OnePoleHPFilter(double initCutoffFreq = 10.0, double initSampleRate = 32000.0);
    OnePoleHPFilter(double initCutoffFreq = 10.0, double initSampleRate = 32000.0) {
        setSampleRate(initSampleRate);
        setCutoffFreq(initCutoffFreq);
        clear();
    }

    #pragma GCC push_options
    #pragma GCC optimize ("Ofast")

    inline double process() {
        _x0 = input;
        _y0 = _a0 * _x0 + _a1 * _x1 + _b1 * _y1;
        _y1 = _y0;
        _x1 = _x0;
        output = _y0;
        return _y0;
    }

    #pragma GCC pop_options

    void clear() {
        input = 0.0;
        output = 0.0;
        _x0 = 0.0;
        _x1 = 0.0;
        _y0 = 0.0;
        _y1 = 0.0;
    }

    #pragma GCC push_options
    #pragma GCC optimize ("Ofast")

    // inline void setCutoffFreq(const double &cutoffFreq) {
    //     // if (cutoffFreq == _cutoffFreq) {
    //     //     return;
    //     // }
        
    //     _cutoffFreq = cutoffFreq;
    //     // See above.
    //     _b1 = _cutoffFreq + 1.;
    //     // _b1 = (0.361 * _cutoffFreq) + 1.;
    //     // _b1 = (0.361 * _cutoffFreq * _1_sampleRate_pi) + 1;
    //     // _b1 = expf(-_2M_PI * _cutoffFreq * _1_sampleRate);
    //     // _a0 = (1.0 + _b1) / 2.0;
    //     _a0 = (1.0 + _b1) * 0.5;
    //     _a1 = -_a0;
    // }

    void setCutoffFreq(double cutoffFreq) {
        if (cutoffFreq == _cutoffFreq) {
            return;
        }

        _cutoffFreq = cutoffFreq;
        _b1 = expf(_cutoffFreq * _1_sampleRate_pi);
        _a0 = (1.0 + _b1) / 2.0;
        _a1 = -_a0;
    }

    #pragma GCC pop_options

    void setSampleRate(double sampleRate) {
        _sampleRate = sampleRate;
        _1_sampleRate_pi = (1.0 / sampleRate) * -_2M_PI;
        _maxCutoffFreq = sampleRate / 2.0 - 1.0;
        setCutoffFreq(_cutoffFreq);
        clear();
    }

    double input = 0.0;
    double output = 0.0;
private:
    double _sampleRate = 32000.0;
    double _1_sampleRate_pi = (1.0 / _sampleRate) * -_2M_PI;
    double _cutoffFreq = 0.0;
    double _maxCutoffFreq = _sampleRate / 2.0 - 1.0;
    double _y0 = 0.0;
    double _y1 = 0.0;
    double _x0 = 0.0;
    double _x1 = 0.0;
    double _a0 = 0.0;
    double _a1 = 0.0;
    double _b1 = 0.0;
};