#pragma once

#include "lvgl/lvgl.h"

#include <string>

namespace zclaw {

class FontManager;

class ShellView {
public:
    ShellView() = default;
    ~ShellView();

    ShellView(const ShellView &) = delete;
    ShellView &operator=(const ShellView &) = delete;

    bool create(lv_obj_t *parent, const FontManager *fonts,
                const std::string &avatar_path,
                const std::string &sparkles_path,
                const std::string &send_button_path);
    lv_obj_t *content() const;

private:
    static void root_deleted(lv_event_t *event);
    void destroy_root();
    void release_root();
    void create_top_bar(const FontManager *fonts, const std::string &avatar_path);
    void create_input_bar(const FontManager *fonts, const std::string &sparkles_path,
                          const std::string &send_button_path);

    lv_obj_t *root_ = nullptr;
};

}  // namespace zclaw
