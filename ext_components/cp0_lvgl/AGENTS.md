# AGENTS.md

## Rules

- When adding a new API exposed to external modules, define it in `include/signal_register_plan.h` using the `def_hal_fun` macro, following the same signature style as the existing entries (e.g. `def_hal_fun(void(std::list<std::string>, std::function<void(int, std::string)>), cp0_signal_xxx_api)`).
