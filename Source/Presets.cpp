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
            { revSize, 0.55f }, { revDamp, 0.5f }, { revMix, 0.20f },
            { width, 1.1f }, { tameDepth, 0.35f } } },

        { "Rap Upfront",
          { { polish, 0.7f }, { space, 0.05f },
            { gateThresh, -50.0f },
            { eqLowCut, 120.0f }, { eqMud, -4.0f }, { eqPresence, 4.0f }, { eqAir, 2.0f },
            { dsThresh, -28.0f },
            { compThresh, -26.0f }, { compRatio, 5.0f }, { compAttack, 4.0f }, { compRelease, 0.25f }, { compMakeup, 8.0f },
            { satDrive, 0.30f },
            { dlyOn, 0.0f },
            { revSize, 0.30f }, { revDamp, 0.6f }, { revMix, 0.08f },
            { compMix, 0.85f }, { tameDepth, 0.4f } } },

        { "Airy R&B",
          { { polish, 0.45f }, { space, 0.55f },
            { eqLowCut, 90.0f }, { eqMud, -2.5f }, { eqPresence, 1.0f }, { eqAir, 6.0f },
            { dsThresh, -34.0f }, { dsFreq, 7000.0f },
            { compThresh, -20.0f }, { compRatio, 2.5f }, { compAttack, 15.0f }, { compRelease, 0.8f }, { compMakeup, 4.0f },
            { satDrive, 0.10f },
            { dlyTime, 450.0f }, { dlyFeedback, 0.35f }, { dlyMix, 0.15f },
            { revSize, 0.75f }, { revDamp, 0.35f }, { revMix, 0.30f },
            { dblOn, 1.0f }, { dblAmount, 0.35f }, { dblDetune, 0.4f }, { width, 1.2f }, { tameDepth, 0.4f } } },

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
            { revSize, 0.85f }, { revDamp, 0.4f }, { revMix, 0.30f },
            { dblOn, 1.0f }, { dblAmount, 0.6f }, { dblDetune, 0.6f }, { width, 1.3f }, { compMix, 0.8f } } },

        { "Natural Leveler",
          { { polish, 0.0f }, { space, 0.0f },
            { eqLowCut, 80.0f },
            { compThresh, -18.0f }, { compRatio, 2.0f }, { compAttack, 20.0f }, { compRelease, 1.5f }, { compMakeup, 3.0f },
            { satOn, 0.0f }, { dlyOn, 0.0f },
            { revSize, 0.35f }, { revMix, 0.08f },
            { compMix, 0.7f }, { tameDepth, 0.2f } } },
    };
    return presets;
}
} // namespace fckrp
