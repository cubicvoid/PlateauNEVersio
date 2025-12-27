#pragma once

#include "bogaudio.hpp"
#include "dsp/filters/utility.hpp"
#include "dsp/signal.hpp"

using namespace bogaudio::dsp;

namespace bogaudio {

struct Lmtr {

	struct Engine {
		float thresholdDb = 0.0f;
		float outGain = -1.0f;
		float outLevel = 0.0f;
		float lastEnv = 0.0f;

		bogaudio::dsp::SlewLimiter attackSL;
		bogaudio::dsp::SlewLimiter releaseSL;
		RootMeanSquare detector;
		Compressor compressor;
		Amplifier amplifier;
		Saturator saturator;

		void sampleRateChange();
	};

	static constexpr float defaultAttackMs = 150.0f;
	static constexpr float maxAttackMs = 5000.0f;
	static constexpr float defaultReleaseMs = 600.0f;
	static constexpr float maxReleaseMs = 20000.0f;

	Engine engine;
	bool _softKnee = true;
	float _attackMs = defaultAttackMs;
	float _releaseMs = defaultReleaseMs;
	float _thresholdRange = 1.0f;

	Lmtr() {
	}

	void sampleRateChange();
	// void modulate();
	// void modulateChannel();
	void init();
	void processChannel(double leftIn, double rightIn, double &rightOut, double &leftOut) ;
};

}