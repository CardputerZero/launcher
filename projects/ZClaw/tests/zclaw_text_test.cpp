#include "zclaw_text.h"

#include <cassert>

int main()
{
    assert(zclaw::trim_ascii_whitespace("").empty());
    assert(zclaw::trim_ascii_whitespace(" \t\r\n").empty());
    assert(zclaw::trim_ascii_whitespace("plain") == "plain");
    assert(zclaw::trim_ascii_whitespace(" \t value with spaces \r\n") ==
           "value with spaces");
    assert(zclaw::trim_ascii_whitespace("a\tb") == "a\tb");
    return 0;
}
