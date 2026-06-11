#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hal_pty *hal_pty_t;

hal_pty_t hal_pty_open(const char *cmd, const char *const *args,
                       int cols, int rows);
int       hal_pty_read(hal_pty_t pty, char *buf, size_t buf_size);
int       hal_pty_write(hal_pty_t pty, const char *buf, size_t len);
int       hal_pty_check_child(hal_pty_t pty, int *exit_status);
void      hal_pty_close(hal_pty_t pty);

#ifdef __cplusplus
}
#endif
