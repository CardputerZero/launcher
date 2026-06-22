#include <main.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    // Allow forcing the OOBE for previews/demos on an already-configured device
    // (e.g. LAUNCH_WIZARD_FORCE=1 or passing --force).
    const char *force_env = getenv("LAUNCH_WIZARD_FORCE");
    bool force = (force_env && force_env[0] && strcmp(force_env, "0") != 0);
    if (argc > 1 && strcmp(argv[1], "--force") == 0)
        force = true;

    if (!force && !launch_wizard_should_run()) {
        printf("LaunchWizard: device already configured, skipping OOBE\n");
        return 0;
    }

    return lvgl_main();
}
