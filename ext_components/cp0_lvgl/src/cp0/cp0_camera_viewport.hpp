#pragma once

#include <string>

namespace cp0_camera {

struct CropRectangle
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

class Viewport
{
public:
    static constexpr int DEFAULT_ZOOM_PERCENT = 100;
    static constexpr int MEDIUM_ZOOM_PERCENT = 250;
    static constexpr int MAX_ZOOM_PERCENT = 500;
    static constexpr int CENTER_PERCENT = 50;
    static constexpr int PAN_STEP_PERCENT = 8;

    void zoom_in();
    void zoom_out();
    void pan(int horizontal_steps, int vertical_steps);

    int zoom_percent() const;
    int view_x_percent() const;
    int view_y_percent() const;
    std::string status_text() const;
    CropRectangle crop(const CropRectangle &bounds) const;

private:
    int zoom_percent_ = DEFAULT_ZOOM_PERCENT;
    int view_x_percent_ = CENTER_PERCENT;
    int view_y_percent_ = CENTER_PERCENT;
};

} // namespace cp0_camera
