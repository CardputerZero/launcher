#include "zclaw_risk_profile_model.h"

#include <cassert>

int main()
{
    zclaw::RiskProfileUpdate update =
        zclaw::add_default_risk_profile("[gateway]\nport = 42617");
    assert(update.changed);
    assert(update.contents ==
           "[gateway]\nport = 42617\n\n[risk_profiles.default]\n");

    const zclaw::RiskProfileUpdate unchanged =
        zclaw::add_default_risk_profile(update.contents);
    assert(!unchanged.changed);
    assert(unchanged.contents == update.contents);

    assert(!zclaw::add_default_risk_profile(
                "  [risk_profiles.default]  \nlevel = \"safe\"\n")
                .changed);
    assert(zclaw::add_default_risk_profile("[risk_profiles.default-extra]\n")
               .changed);
    assert(zclaw::add_default_risk_profile("").contents ==
           "\n[risk_profiles.default]\n");
    assert(zclaw::add_default_risk_profile("[gateway]\n").contents ==
           "[gateway]\n\n[risk_profiles.default]\n");
    return 0;
}
