#include "rtc_ntp_state.hpp"

namespace setting {

NtpToggleEligibility ntp_toggle_eligibility(bool in_flight, bool dirty, bool available)
{
    if (in_flight) return NtpToggleEligibility::IN_FLIGHT;
    if (dirty) return NtpToggleEligibility::DIRTY;
    if (!available) return NtpToggleEligibility::UNAVAILABLE;
    return NtpToggleEligibility::ALLOWED;
}

bool ntp_rollback_value(bool previous)
{
    return previous;
}

PrivilegedResultKind classify_privileged_result(int result)
{
    switch (result) {
    case 0: return PrivilegedResultKind::SUCCESS;
    case 1: return PrivilegedResultKind::AUTH_FAILED;
    case 2: return PrivilegedResultKind::EXEC_FAILED;
    case 3: return PrivilegedResultKind::CANCELLED;
    case 4: return PrivilegedResultKind::TIMED_OUT;
    default: return PrivilegedResultKind::EXEC_FAILED;
    }
}

} // namespace setting
