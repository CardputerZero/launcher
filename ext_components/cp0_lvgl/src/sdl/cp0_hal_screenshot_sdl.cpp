#include "hal/hal_screenshot.h"
#include <stdio.h>

int hal_screenshot_save(const char *dir)
{
    (void)dir;
    printf("[SCREENSHOT] Not supported on SDL platform\n");
    return -1;
}
