# LaunchWizard Engineering Rules

## UI Typography

- The minimum UI font size is **12 px**. Do not introduce any font size below
  12 px in LaunchWizard screens, labels, controls, dialogs, or status text.
- Keep the FreeType font size table and built-in LVGL fallbacks at 12 px or
  larger. When changing UI layout or text, verify that the rendered text still
  uses a font of at least 12 px and remains inside its parent bounds.
