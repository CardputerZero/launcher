#include "cp0_camera_viewport.hpp"

#include <cassert>
#include <climits>

using cp0_camera::CropRectangle;
using cp0_camera::Viewport;

int main()
{
    Viewport viewport;
    assert(viewport.zoom_percent() == 100);
    assert(viewport.view_x_percent() == 50);
    assert(viewport.view_y_percent() == 50);
    assert(viewport.status_text() == "ZOOM 100 50 50\n");

    const CropRectangle bounds{10, 20, 1000, 800};
    CropRectangle crop = viewport.crop(bounds);
    assert(crop.x == 10 && crop.y == 20 && crop.width == 1000 && crop.height == 800);

    viewport.pan(2, -2);
    assert(viewport.view_x_percent() == 50);
    assert(viewport.view_y_percent() == 50);

    viewport.zoom_in();
    assert(viewport.zoom_percent() == 250);
    crop = viewport.crop(bounds);
    assert(crop.x == 310 && crop.y == 260 && crop.width == 400 && crop.height == 320);

    viewport.pan(INT_MAX, INT_MIN);
    assert(viewport.view_x_percent() == 100);
    assert(viewport.view_y_percent() == 0);
    crop = viewport.crop(bounds);
    assert(crop.x == 610 && crop.y == 20);

    viewport.zoom_in();
    assert(viewport.zoom_percent() == 500);
    crop = viewport.crop(bounds);
    assert(crop.x == 810 && crop.y == 20 && crop.width == 200 && crop.height == 160);

    viewport.zoom_out();
    assert(viewport.zoom_percent() == 250);
    assert(viewport.view_x_percent() == 100);
    assert(viewport.view_y_percent() == 0);
    viewport.zoom_out();
    assert(viewport.zoom_percent() == 100);
    assert(viewport.view_x_percent() == 50);
    assert(viewport.view_y_percent() == 50);

    const CropRectangle invalid{3, 4, 0, -1};
    crop = viewport.crop(invalid);
    assert(crop.x == 3 && crop.y == 4 && crop.width == 0 && crop.height == -1);

    viewport.zoom_in();
    crop = viewport.crop({0, 0, INT_MAX, INT_MAX});
    assert(crop.width == INT_MAX * 2LL / 5);
    assert(crop.x == (INT_MAX - crop.width) / 2);
}
