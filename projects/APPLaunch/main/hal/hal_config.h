#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void hal_config_init(void);
int  hal_config_get_int(const char *key, int default_val);
void hal_config_set_int(const char *key, int val);
const char *hal_config_get_str(const char *key, const char *default_val);
void hal_config_set_str(const char *key, const char *val);
void hal_config_save(void);

#ifdef __cplusplus
}
#endif
