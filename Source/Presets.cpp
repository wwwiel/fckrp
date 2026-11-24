#include "Presets.h"
#include "Parameters.h"

namespace fckrp
{
using namespace ids;

const std::vector<Preset>& getFactoryPresets()
{
    static const std::vector<Preset> presets {
        { "Init", {} },

        { "Clean Pop",
          { { polish, 0.5f }, { space, 0.3f },
            { eqLowCut, 100.0f }, { eqMud, -2.0f }, { eqPresence, 2.0f }, { eqAir, 3.0f },
            { dsThresh, -30.0f },
            { compThresh, -22.0f }, { compRatio, 3.0f }, { compAttack, 10.0f }, { compRelease, 0.4f }, { compMakeup, 5.0f },
            { satDrive, 0.15f },
            { dlyTime, 375.0f }, { dlyFeedback, 0.25f }, { dlyMix, 0.10f },
            { revSize, 0.55f }, { revDamp, 0.5f }, { revMix, 0.20f } } },

        { "Rap Upfront",
          { { polish, 0.7f }, { space, 0.05f },
            { gateThresh, -50.0f },
            { eqLowCut, 120.0f }, { eqMud, -4.0f }, { eqPresence, 4.0f }, { eqAir, 2.0f },
            { dsThresh, -28.0f },
            { compThresh, -26.0f }, { compRatio, 5.0f }, { compAttack, 4.0f }, { compRelease, 0.25f }, { compMakeup, 8.0f },
            { satDrive, 0.30f },
            { dlyOn, 0.0f },
            { revSize, 0.30f }, { revDamp, 0.6f }, { revMix, 0.08f } } },

        { "Airy R&B",
          { { polish, 0.45f }, { space, 0.55f },
            { eqLowCut, 90.0f }, { eqMud, -2.5f }, { eqPresence, 1.0f }, { eqAir, 6.0f },
            { dsThresh, -34.0f }, { dsFreq, 7000.0f },
            { compThresh, -20.0f }, { compRatio, 2.5f }, { compAttack, 15.0f }, { compRelease, 0.8f }, { compMakeup, 4.0f },
            { satDrive, 0.10f },
            { dlyTime, 450.0f }, { dlyFeedback, 0.35f }, { dlyMix, 0.15f },
            { revSize, 0.75f }, { revDamp, 0.35f }, { revMix, 0.30f } } },

        { "Lo-Fi Warm",
          { { polish, 0.3f }, { space, 0.3f },
            { eqLowCut, 200.0f }, { eqMud, 1.0f }, { eqPresence, 3.0f }, { eqAir, -5.0f },
            { compThresh, -24.0f }, { compRatio, 4.0f }, { compAttack, 20.0f }, { compRelease, 1.0f }, { compMakeup, 6.0f },
            { satDrive, 0.65f },
            { dlyTime, 250.0f }, { dlyFeedback, 0.30f }, { dlyMix, 0.12f },
            { revSize, 0.40f }, { revDamp, 0.8f }, { revMix, 0.15f } } },

        { "Big Hook",
          { { polish, 0.6f }, { space, 0.7f },
            { eqLowCut, 110.0f }, { eqMud, -3.0f }, { eqPresence, 3.0f }, { eqAir, 4.0f },
            { dsThresh, -30.0f },
            { compThresh, -24.0f }, { compRatio, 4.0f }, { compAttack, 8.0f }, { compRelease, 0.5f }, { compMakeup, 6.0f },
            { satDrive, 0.20f },
            { dlyTime, 500.0f }, { dlyFeedback, 0.40f }, { dlyMix, 0.18f },
            { revSize, 0.85f }, { revDamp, 0.4f }, { revMix, 0.30f } } },

        { "Natural Leveler",
          { { polish, 0.0f }, { space, 0.0f },
            { eqLowCut, 80.0f },
            { compThresh, -18.0f }, { compRatio, 2.0f }, { compAttack, 20.0f }, { compRelease, 1.5f }, { compMakeup, 3.0f },
            { satOn, 0.0f }, { dlyOn, 0.0f },
            { revSize, 0.35f }, { revMix, 0.08f } } },

        // Tuned on Lukas's own raw vocal by rendering it offline and
        // measuring: the raw take is quiet (about -28 dBFS), dark and boxy
        // (lots of 250-500 Hz, little 2-8 kHz) with loud S sounds. This makes
        // it a thick, dense, upfront lead that stays in the centre.
        { "Lukas Lead",
          { { input, 12.0f }, { polish, 0.0f }, { space, 0.0f },
            { gateThresh, -36.0f },
            { eqLowCut, 75.0f }, { eqBody, 3.0f }, { eqMud, -7.0f }, { eqPresence, 8.0f }, { eqAir, 8.0f },
            { dsLate, 1.0f }, { dsFreq, 5500.0f }, { dsThresh, -32.0f },
            { compThresh, -24.0f }, { compRatio, 4.0f }, { compAttack, 3.0f }, { compRelease, 0.15f }, { compMakeup, 6.0f }, { compMix, 0.85f },
            { satDrive, 0.5f }, { satChar, 1.0f },
            { punchOn, 1.0f }, { punchAmount, 0.7f },
            { dlyOn, 1.0f }, { dlyTime, 375.0f }, { dlyFeedback, 0.2f }, { dlyMix, 0.06f },
            { revSize, 0.35f }, { revDamp, 0.6f }, { revMix, 0.06f } } },

        // Same voice as a second vocal / double: wider, thinner at the
        // bottom and tucked behind the lead.
        { "Lukas Second",
          { { input, 12.0f }, { polish, 0.0f }, { space, 0.0f }, { output, -2.0f },
            { gateThresh, -34.0f },
            { eqLowCut, 150.0f }, { eqBody, 0.0f }, { eqMud, -7.0f }, { eqPresence, 5.0f }, { eqAir, 6.0f },
            { dsLate, 1.0f }, { dsFreq, 5500.0f }, { dsThresh, -40.0f },
            { compThresh, -28.0f }, { compRatio, 6.0f }, { compAttack, 3.0f }, { compRelease, 0.12f }, { compMakeup, 6.0f },
            { satDrive, 0.5f }, { satChar, 1.0f },
            { punchOn, 1.0f }, { punchAmount, 0.6f },
            { dblOn, 1.0f }, { dblAmount, 0.7f }, { dblDetune, 0.6f }, { width, 1.4f },
            { dlyOn, 1.0f }, { dlyTime, 375.0f }, { dlyFeedback, 0.25f }, { dlyMix, 0.1f },
            { revSize, 0.45f }, { revDamp, 0.6f }, { revMix, 0.12f } } },
    };
    return presets;
}
} // namespace fckrp
