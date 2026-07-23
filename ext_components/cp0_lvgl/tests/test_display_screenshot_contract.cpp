#include "cp0_display_screenshot_contract.hpp"

#include <cassert>
#include <limits>
#include <string>

namespace {

void test_display_dimensions()
{
    assert(cp0_display_dimension_or_default(nullptr, 320) == 320);
    assert(cp0_display_dimension_or_default("", 320) == 320);
    assert(cp0_display_dimension_or_default("1", 320) == 1);
    assert(cp0_display_dimension_or_default("16384", 320) == 16384);
    assert(cp0_display_dimension_or_default("0", 320) == 320);
    assert(cp0_display_dimension_or_default("-1", 320) == 320);
    assert(cp0_display_dimension_or_default("16385", 320) == 320);
    assert(cp0_display_dimension_or_default("320junk", 170) == 170);
    assert(cp0_display_dimension_or_default("999999999999999999", 170) == 170);
}

void test_request_and_output_path()
{
    cp0::screenshot::Request request;
    assert(!cp0::screenshot::parse_request({}, request));
    assert(!cp0::screenshot::parse_request({"Save"}, request));
    assert(!cp0::screenshot::parse_request({"Save", ""}, request));
    assert(!cp0::screenshot::parse_request({"Save", "/tmp", "junk"}, request));
    assert(!cp0::screenshot::parse_request({"Other", "/tmp"}, request));
    assert(cp0::screenshot::parse_request({"Save", "/tmp/screens"}, request));
    assert(request.directory == "/tmp/screens");
    assert(!cp0::screenshot::parse_request({"Save", std::string(512, 'x')}, request));

    const cp0::screenshot::Timestamp timestamp{2026, 7, 22, 9, 8, 7};
    std::string path;
    assert(cp0::screenshot::make_output_path("/tmp/screens", timestamp, false, path));
    assert(path == "/tmp/screens/scr_20260722_090807.bmp");
    assert(cp0::screenshot::make_output_path("/tmp/screens", timestamp, true, path));
    assert(path == "/tmp/screens/scr_20260722_090807_sdl.bmp");
    assert(!cp0::screenshot::make_output_path("", timestamp, false, path));
    assert(!cp0::screenshot::make_output_path("/tmp", {2026, 13, 1, 0, 0, 0}, false, path));
    assert(!cp0::screenshot::make_output_path(std::string(500, 'x'), timestamp, false, path));
}

void test_framebuffer_layout()
{
    cp0::screenshot::FramebufferLayout layout;
    assert(cp0::screenshot::framebuffer_layout(320, 170, 16, 640, layout));
    assert(layout.mapped_size == 108800);
    assert(layout.bmp_row_size == 960);
    assert(layout.bmp_image_size == 163200);
    assert(layout.bmp_file_size == 163254);

    assert(cp0::screenshot::framebuffer_layout(1, 1, 32, 4, layout));
    assert(layout.mapped_size == 4 && layout.bmp_row_size == 4);
    assert(!cp0::screenshot::framebuffer_layout(0, 170, 16, 640, layout));
    assert(!cp0::screenshot::framebuffer_layout(320, -1, 16, 640, layout));
    assert(!cp0::screenshot::framebuffer_layout(320, 170, 24, 960, layout));
    assert(!cp0::screenshot::framebuffer_layout(320, 170, 16, 639, layout));
    assert(!cp0::screenshot::framebuffer_layout(16385, 1, 32, 65540, layout));
    assert(!cp0::screenshot::valid_dimensions(1, 0));
    assert(cp0::screenshot::valid_dimensions(16384, 16384));
}

} // namespace

int main()
{
    test_display_dimensions();
    test_request_and_output_path();
    test_framebuffer_layout();
}
