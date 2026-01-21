#include "GainState.hpp"

#include "Distortion.hpp"
#include "Parameters.hpp"

namespace campestria {

void GainState::Apply(const Parameters &params) {
  mode = params.GainMode();
  gainMod = params.outputAmplification;

  if (mode == 0) {
    softerLimiterLeft.limit = 0.85 * gainMod;
    softerLimiterRight.limit = 0.85 * gainMod;
  } else {
    softerLimiterLeft.limit = 0.85;
    softerLimiterRight.limit = 0.85;
  }

  const double rippedSpeakerThreshold = 2.0 * gainMod * gainMod;
  rippedSpeakerLeft.SetThreshold(rippedSpeakerThreshold);
  rippedSpeakerRight.SetThreshold(rippedSpeakerThreshold);
}

void GainState::gainControl(double *left, double *right) {
  double mix = 1. - gainMod * gainMod * gainMod;
  switch (mode) {
  case 0: {
    // Regular soft limiter. Rarely clips. Lower limit threshold by
    // turning tone knob up output dynamic setting selected.
    break;
  }
  case 1: {
    // Same clipper as VCV rack. Lower clip threshold by turning tone knob
    // up with output dynamic setting selected.
    const double clipLimit = gainMod * gainMod;
    const double modifier = 1. + (gainMod + 0.2) / (clipLimit + 0.000000001);
    *left = modifier * hardClip(*left, clipLimit);
    *right = modifier * hardClip(*right, clipLimit);
  } break;
  case 2: {
    // Same as last but with saturation
    const double clipLimit = gainMod * gainMod;
    const double modifier = 1. + (gainMod + 0.2) / (clipLimit + 0.000000001);

    double leftClipped = modifier * hardClip(*left, clipLimit);
    double rightClipped = modifier * hardClip(*right, clipLimit);

    double saturatedLeft = leftClipped;
    double saturatedRight = rightClipped;
    saturatedLeft *= 1. + (1 - gainMod) * 20.;
    saturatedRight *= 1. + (1 - gainMod) * 20.;
    saturatedLeft = saturation(saturatedLeft);
    saturatedLeft = saturation(saturatedRight);
    saturatedLeft *= 1 - 1.6 * mix + 0.83 * mix * mix;
    saturatedRight *= 1 - 1.6 * mix + 0.83 * mix * mix;

    *left = leftClipped * (1. - mix) + saturatedLeft * mix;
    *right = rightClipped * (1. - mix) + saturatedRight * mix;
    hardLimiter(left, right);

  } break;
  case 3: {
    // Just saturation. Control gain going into saturation with tone knob
    // output dynamic setting
    double saturatedLeft = *left;
    double saturatedRight = *right;
    saturatedLeft *= 1. + (1 - gainMod) * 20.;
    saturatedRight *= 1. + (1 - gainMod) * 20.;
    saturatedLeft = saturation(saturatedLeft);
    saturatedRight = saturation(saturatedRight);
    saturatedLeft *= 1 - 1.6 * mix + 0.83 * mix * mix;
    saturatedRight *= 1 - 1.6 * mix + 0.83 * mix * mix;
    // saturatedLeft *= 1. + mix * mix * mix * mix;
    // saturatedRight *= 1. + mix * mix * mix * mix;
    *left = *left * (1. - mix) + saturatedLeft * mix;
    *right = *right * (1. - mix) + saturatedRight * mix;
    hardLimiter(left, right);

  } break;
  case 4: {
    // Bogaudio LMTR then stock VCV clip
    hardLimiter(left, right);
    const double clipLimit = gainMod * gainMod;
    const double modifier = 1. + (gainMod + 0.2) / (clipLimit + 0.000000001);
    *left = modifier * hardClip(*left, clipLimit);
    *right = modifier * hardClip(*right, clipLimit);
  } break;
  case 5: {
    // Stock VCV clip then Bogaudio LMTR
    const double clipLimit = gainMod * gainMod;
    const double modifier = 1. + (gainMod + 0.2) / (clipLimit + 0.000000001);

    *left = modifier * hardClip(*left, clipLimit);
    *right = modifier * hardClip(*right, clipLimit);
    hardLimiter(left, right);
  } break;
  case 6: {
    // Foldback distortion. Full wave rectifier that folds back on itself
    campestria::foldbackDistortion(*left, gainMod);
    campestria::foldbackDistortion(*right, gainMod);
    hardLimiter(left, right);
  } break;
  case 7: {
    // Output to zero once past threshold. Simulates ripped speaker
    double saturatedLeft = rippedSpeakerLeft.process(*left);
    double saturatedRight = rippedSpeakerRight.process(*right);
    mix *= 2;
    if (mix > 1.) {
      mix = 1.;
    }
    *left = *left * (1. - mix) + saturatedLeft * mix;
    *left = *right * (1. - mix) + saturatedRight * mix;
    hardLimiter(left, right);

  } break;
  case 8: {
    // Same as last but saturation before ripped speaker
    double saturatedLeft = rippedSpeakerLeft.process(*left);
    double saturatedRight = rippedSpeakerRight.process(*right);
    saturatedLeft *= 1. + (1 - gainMod) * 15.;
    saturatedRight *= 1. + (1 - gainMod) * 15.;
    saturatedLeft = saturation(saturatedLeft);
    saturatedRight = saturation(saturatedRight);
    saturatedLeft *= 1 - 1.6 * mix + 0.83 * mix * mix;
    saturatedRight *= 1 - 1.6 * mix + 0.83 * mix * mix;
    mix *= 2;
    if (mix > 1.) {
      mix = 1.;
    }
    *left = *left * (1. - mix) + saturatedLeft * mix;
    *right = *right * (1. - mix) + saturatedRight * mix;
    hardLimiter(left, right);

  } break;
  case 9: {
    // Same as last but in addition to saturation there is also a hard
    // clipper
    const double clipLimit = gainMod * gainMod + 0.1;
    const double modifier =
        1. + (gainMod + 0.2) / (clipLimit - 0.1 + 0.000000001);
    *left = modifier * hardClip(*left, clipLimit);
    *right = modifier * hardClip(*right, clipLimit);

    double saturatedLeft = rippedSpeakerLeft.process(*left);
    double saturatedRight = rippedSpeakerRight.process(*right);
    saturatedLeft *= 1. + (1 - gainMod) * 15.;
    saturatedRight *= 1. + (1 - gainMod) * 15.;
    saturatedLeft = saturation(saturatedLeft);
    saturatedRight = saturation(saturatedRight);
    saturatedLeft *= 1 - 1.6 * mix + 0.83 * mix * mix;
    saturatedRight *= 1 - 1.6 * mix + 0.83 * mix * mix;

    mix = std::min(mix * 2, 1.0);

    *left = *left * (1. - mix) + saturatedLeft * mix;
    *right = *right * (1. - mix) + saturatedRight * mix;

    hardLimiter(left, right);

  } break;
  case 10: {
    // Last one is Bogaudio LMTR followed by the ripped speaker
    hardLimiter(left, left, -30.0f);
    double saturatedLeft = rippedSpeakerLeft.process(*left);
    double saturatedRight = rippedSpeakerRight.process(*right);
    mix *= 2;
    if (mix > 1.) {
      mix = 1.;
    }
    *left = *left * (1. - mix) + saturatedLeft * mix;
    *right = *right * (1. - mix) + saturatedRight * mix;
  } break;
  }

  *left = softerLimiterLeft.sample(*left);
  *right = softerLimiterRight.sample(*right);
}

} // namespace campestria