#pragma once

namespace setting {

enum class NtpToggleEligibility { ALLOWED, IN_FLIGHT, DIRTY, UNAVAILABLE };

NtpToggleEligibility ntp_toggle_eligibility(bool in_flight, bool dirty, bool available);
bool ntp_rollback_value(bool previous);

enum class PrivilegedResultKind { SUCCESS, AUTH_FAILED, CANCELLED, TIMED_OUT, EXEC_FAILED };

PrivilegedResultKind classify_privileged_result(int result);

} // namespace setting
