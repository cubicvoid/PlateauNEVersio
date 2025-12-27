
#include "Lmtr.hpp"

#define ATTACK_MS "attack_ms"
#define RELEASE_MS "release_ms"
#define THRESHOLD_RANGE "threshold_range"

void Lmtr::Engine::sampleRateChange() {
	detector.setSampleRate(32000);
}

void Lmtr::sampleRateChange() {
		engine.sampleRateChange();
}

// float thresholdParam;
// float outGainParam;
// float kneeParam;

// template<typename _Tp>
// constexpr const _Tp&
// clamp(const _Tp& __val, const _Tp& __lo, const _Tp& __hi)
// {
// 	__glibcxx_assert(!(__hi < __lo));
// 	return std::min(std::max(__val, __lo), __hi);
// }

// void Lmtr::modulate() {
// 	_softKnee = kneeParam > 0.5f;
// }


// void Lmtr::modulateChannel() {
// 	Engine& e = *engine;

// 	e.thresholdDb = thresholdParam;
// 	e.thresholdDb *= clamp(thresholdParam / 10.0f, 0.0f, 1.0f);
// 	e.thresholdDb *= 30.0f;
// 	e.thresholdDb -= 24.0f;
// 	e.thresholdDb *= _thresholdRange;

// 	float outGain = outGainParam;
// 	outGain = clamp(outGain + outGainParam / 5.0f, 0.0f, 1.0f);
// 	outGain *= 24.0f;
// 	if (e.outGain != outGain) {
// 		e.outGain = outGain;
// 		e.outLevel = decibelsToAmplitude(e.outGain);
// 	}

// 	float sr = 32000;
// 	e.attackSL.setParams(sr, _attackMs);
// 	e.releaseSL.setParams(sr, _releaseMs);
// }

void Lmtr::init() {
	sampleRateChange();
	engine.thresholdDb = -30.;
	engine.outLevel = 1.;
	float sr = 32000;
	engine.attackSL.setParams(sr, defaultAttackMs);
	engine.releaseSL.setParams(sr, defaultReleaseMs);
	_softKnee = false;
}

void Lmtr::processChannel(double leftIn, double rightIn, double &rightOut, double &leftOut) {
	float env = engine.detector.next(leftIn + rightIn);
	if (env > engine.lastEnv) {
		env = engine.attackSL.next(env, engine.lastEnv);
	}
	else {
		env = engine.releaseSL.next(env, engine.lastEnv);
	}
	engine.lastEnv = env;

	float detectorDb = amplitudeToDecibels(env / 5.0f);
	float compressionDb = engine.compressor.compressionDb(detectorDb, engine.thresholdDb, Compressor::maxEffectiveRatio, _softKnee);
	engine.amplifier.setLevel(-compressionDb);
	leftOut = engine.saturator.next(engine.amplifier.next(leftIn) * engine.outLevel);
	rightOut = engine.saturator.next(engine.amplifier.next(rightIn) * engine.outLevel);
}