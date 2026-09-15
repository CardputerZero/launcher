#pragma once

#ifdef __cplusplus
extern "C" {
#endif
int cp0_timedate_get_ntp(int *enabled);
int cp0_timedate_get_time(int values[6]);
int cp0_timedate_set_ntp(int enabled);
int cp0_timedate_set_time(const char *timestamp);
#ifdef __cplusplus
}
#endif
