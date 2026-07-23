#include "zclaw_config_codec.h"

#include <cassert>
#include <string>
#include <vector>

int main()
{
    const std::vector<std::string> values = {
        "",
        "plain",
        "field\\value\tline\nnext",
        "trailing\\",
    };
    for (const std::string &value : values)
        assert(zclaw::decode_config_field(zclaw::encode_config_field(value)) == value);

    assert(zclaw::decode_config_field("unknown\\qescape\\") == "unknownqescape\\");
    assert(zclaw::is_valid_config_field_encoding("slash\\\\tab\\tline\\n"));
    assert(!zclaw::is_valid_config_field_encoding("trailing\\"));
    assert(!zclaw::is_valid_config_field_encoding("unknown\\qescape"));

    const std::vector<std::string> empty = zclaw::split_config_line("");
    assert(empty.size() == 1 && empty[0].empty());

    const std::vector<std::string> fields = zclaw::split_config_line("a\t\tc\t");
    assert(fields.size() == 4);
    assert(fields[0] == "a");
    assert(fields[1].empty());
    assert(fields[2] == "c");
    assert(fields[3].empty());
    return 0;
}
