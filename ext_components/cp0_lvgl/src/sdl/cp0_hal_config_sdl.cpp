#include "hal/hal_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void hal_config_init(void) {}
int  hal_config_get_int(const char *key, int default_val) { (void)key; return default_val; }
void hal_config_set_int(const char *key, int val) { (void)key; (void)val; }
const char *hal_config_get_str(const char *key, const char *default_val) { (void)key; return default_val; }
void hal_config_set_str(const char *key, const char *val) { (void)key; (void)val; }
void hal_config_save(void) {}
