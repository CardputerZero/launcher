#include "manual_datetime_validation.h"

#include <assert.h>

#include <string>

int main()
{
    std::string error;
    assert(launch_wizard::validate_manual_datetime("2026-07-24", "00:00", error));
    assert(launch_wizard::validate_manual_datetime("2024-02-29", "23:59", error));
    assert(!launch_wizard::validate_manual_datetime("2023-02-29", "12:00", error));
    assert(!launch_wizard::validate_manual_datetime("2026-04-31", "12:00", error));
    assert(!launch_wizard::validate_manual_datetime("2026/07/24", "12:00", error));
    assert(!launch_wizard::validate_manual_datetime("2026-07-24", "24:00", error));
    assert(!launch_wizard::validate_manual_datetime("2026-07-24", "12:60", error));
    assert(!launch_wizard::validate_manual_datetime("2026-07-24", "9:30", error));
    return 0;
}
