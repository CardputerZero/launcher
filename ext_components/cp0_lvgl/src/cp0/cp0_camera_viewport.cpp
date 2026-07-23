#include "cp0_camera_viewport.hpp"

#include <algorithm>
#include <cstdio>
#include <limits>

namespace cp0_camera {
namespace {

int pan_position(int position, int steps)
{
    const long long moved = static_cast<long long>(position) +
                            static_cast<long long>(steps) * Viewport::PAN_STEP_PERCENT;
    return static_cast<int>(std::max(0LL, std::min(100LL, moved)));
}

int scaled_dimension(int dimension, int zoom_percent)
{
    const long long scaled = static_cast<long long>(dimension) * 100 / zoom_percent;
    return static_cast<int>(std::max(1LL, std::min(
                                                  static_cast<long long>(std::numeric_limits<int>::max()),
                                                  scaled)));
}

int position_offset(int available, int position_percent)
{
    return static_cast<int>(static_cast<long long>(available) * position_percent / 100);
}

} // namespace

void Viewport::zoom_in()
{
    zoom_percent_ = zoom_percent_ < MEDIUM_ZOOM_PERCENT ? MEDIUM_ZOOM_PERCENT : MAX_ZOOM_PERCENT;
}

void Viewport::zoom_out()
{
    zoom_percent_ = zoom_percent_ > MEDIUM_ZOOM_PERCENT ? MEDIUM_ZOOM_PERCENT : DEFAULT_ZOOM_PERCENT;
    if (zoom_percent_ == DEFAULT_ZOOM_PERCENT)
    {
        view_x_percent_ = CENTER_PERCENT;
        view_y_percent_ = CENTER_PERCENT;
    }
}

void Viewport::pan(int horizontal_steps, int vertical_steps)
{
    if (zoom_percent_ == DEFAULT_ZOOM_PERCENT)
        return;

    view_x_percent_ = pan_position(view_x_percent_, horizontal_steps);
    view_y_percent_ = pan_position(view_y_percent_, vertical_steps);
}

int Viewport::zoom_percent() const
{
    return zoom_percent_;
}

int Viewport::view_x_percent() const
{
    return view_x_percent_;
}

int Viewport::view_y_percent() const
{
    return view_y_percent_;
}

std::string Viewport::status_text() const
{
    char buffer[96];
    std::snprintf(buffer, sizeof(buffer), "ZOOM %d %d %d\n",
                  zoom_percent_, view_x_percent_, view_y_percent_);
    return std::string(buffer);
}

CropRectangle Viewport::crop(const CropRectangle &bounds) const
{
    if (zoom_percent_ == DEFAULT_ZOOM_PERCENT || bounds.width <= 0 || bounds.height <= 0)
        return bounds;

    const int crop_width = scaled_dimension(bounds.width, zoom_percent_);
    const int crop_height = scaled_dimension(bounds.height, zoom_percent_);
    const int available_x = std::max(0, bounds.width - crop_width);
    const int available_y = std::max(0, bounds.height - crop_height);

    return {
        bounds.x + position_offset(available_x, view_x_percent_),
        bounds.y + position_offset(available_y, view_y_percent_),
        crop_width,
        crop_height,
    };
}

} // namespace cp0_camera
