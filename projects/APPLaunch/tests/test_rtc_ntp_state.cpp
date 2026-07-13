#include "../main/ui/page_app/setting/rtc_ntp_state.hpp"
#include <cassert>
int main()
{
    using namespace setting;
    assert(ntp_toggle_eligibility(true, false, true) == NtpToggleEligibility::IN_FLIGHT);
    assert(ntp_toggle_eligibility(false, true, true) == NtpToggleEligibility::DIRTY);
    assert(ntp_toggle_eligibility(false, false, false) == NtpToggleEligibility::UNAVAILABLE);
    assert(ntp_toggle_eligibility(false, false, true) == NtpToggleEligibility::ALLOWED);
    assert(ntp_rollback_value(true) && !ntp_rollback_value(false));
    assert(classify_privileged_result(0) == PrivilegedResultKind::SUCCESS);
    assert(classify_privileged_result(1) == PrivilegedResultKind::AUTH_FAILED);
    assert(classify_privileged_result(2) == PrivilegedResultKind::EXEC_FAILED);
    assert(classify_privileged_result(3) == PrivilegedResultKind::CANCELLED);
    assert(classify_privileged_result(4) == PrivilegedResultKind::TIMED_OUT);
}
