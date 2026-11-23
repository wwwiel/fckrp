#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace fckrp::ids
{
// Parameter IDs. Never rename these after release: hosts and saved
// projects refer to them.
inline constexpr const char* polish      = "polish";
inline constexpr const char* space       = "space";
inline constexpr const char* output      = "output";

inline constexpr const char* gateOn      = "gate_on";
inline constexpr const char* gateThresh  = "gate_thresh";

inline constexpr const char* eqOn        = "eq_on";
inline constexpr const char* eqLowCut    = "eq_lowcut";
inline constexpr const char* eqMud       = "eq_mud";
inline constexpr const char* eqPresence  = "eq_presence";
inline constexpr const char* eqAir       = "eq_air";

inline constexpr const char* dsOn        = "ds_on";
inline constexpr const char* dsFreq      = "ds_freq";
inline constexpr const char* dsThresh    = "ds_thresh";

inline constexpr const char* compOn      = "comp_on";
inline constexpr const char* compThresh  = "comp_thresh";
inline constexpr const char* compRatio   = "comp_ratio";
inline constexpr const char* compAttack  = "comp_attack";
inline constexpr const char* compRelease = "comp_release";
inline constexpr const char* compMakeup  = "comp_makeup";

inline constexpr const char* satOn       = "sat_on";
inline constexpr const char* satDrive    = "sat_drive";
inline constexpr const char* satChar     = "sat_char";

inline constexpr const char* dlyOn       = "dly_on";
inline constexpr const char* dlyTime     = "dly_time";
inline constexpr const char* dlyFeedback = "dly_feedback";
inline constexpr const char* dlyMix      = "dly_mix";

inline constexpr const char* revOn       = "rev_on";
inline constexpr const char* revSize     = "rev_size";
inline constexpr const char* revDamp     = "rev_damp";
inline constexpr const char* revMix      = "rev_mix";

inline constexpr const char* compMix     = "comp_mix";
inline constexpr const char* tameOn      = "tame_on";
inline constexpr const char* tameDepth   = "tame_depth";
inline constexpr const char* tameSens    = "tame_sens";
inline constexpr const char* dblOn       = "dbl_on";
inline constexpr const char* dblAmount   = "dbl_amount";
inline constexpr const char* dblDetune   = "dbl_detune";
inline constexpr const char* width       = "width";

inline constexpr const char* all[] = {
    polish, space, output, gateOn, gateThresh, eqOn, eqLowCut, eqMud, eqPresence, eqAir,
    dsOn, dsFreq, dsThresh, compOn, compThresh, compRatio, compAttack, compRelease, compMakeup,
    satOn, satDrive, satChar, dlyOn, dlyTime, dlyFeedback, dlyMix, revOn, revSize, revDamp, revMix,
    compMix, tameOn, tameDepth, tameSens, dblOn, dblAmount, dblDetune, width
};
} // namespace fckrp::ids

namespace fckrp
{
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
}
