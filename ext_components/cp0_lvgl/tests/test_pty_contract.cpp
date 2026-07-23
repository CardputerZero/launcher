#include "cp0_pty_contract.hpp"

#include <cassert>
#include <cstdint>
#include <limits>

int main()
{
    using cp0::pty::Command;
    using cp0::pty::parse_request;

    auto open = parse_request({"Open", "bash", "120", "40", "bash", "-i"});
    assert(open.ok && open.request.command == Command::open);
    assert(open.request.executable == "bash" && open.request.columns == 120 && open.request.rows == 40);
    assert(open.request.argv.size() == 2 && open.request.argv[0] == "bash" && open.request.argv[1] == "-i");

    auto defaults = parse_request({"Open", "sh"});
    assert(defaults.ok && defaults.request.columns == 80 && defaults.request.rows == 24);
    auto maximum_dimensions = parse_request({"Open", "sh", "65535", "65535"});
    assert(maximum_dimensions.ok && maximum_dimensions.request.columns == 65535 &&
           maximum_dimensions.request.rows == 65535);
    assert(!parse_request({"Open", "sh", "0", "24"}).ok);
    assert(!parse_request({"Open", "sh", "70000", "24"}).ok);
    assert(!parse_request({"Open", "sh", "+80", "24"}).ok);
    assert(!parse_request({"Open", ""}).ok);

    auto read = parse_request({"Read", "42"});
    assert(read.ok && read.request.command == Command::read && read.request.handle == 42);
    assert(read.request.max_read == 4096);
    assert(parse_request({"Read", "42", "1048576"}).ok);
    assert(!parse_request({"Read", "42", "0"}).ok);
    assert(!parse_request({"Read", "42", "1048577"}).ok);
    assert(!parse_request({"Read", "42", "1048576junk"}).ok);
    assert(!parse_request({"Read", "-1"}).ok);
    assert(!parse_request({"Read", "0x2a"}).ok);

    auto write = parse_request({"Write", "42", std::string("a\0b", 3)});
    assert(write.ok && write.request.command == Command::write && write.request.data.size() == 3);
    assert(!parse_request({"Write", "42"}).ok);

    auto resize = parse_request({"Resize", "42", "100", "30"});
    assert(resize.ok && resize.request.command == Command::resize);
    assert(resize.request.columns == 100 && resize.request.rows == 30);
    assert(!parse_request({"Resize", "42", "100", "bad"}).ok);

    assert(parse_request({"CheckChild", "42"}).request.command == Command::check_child);
    assert(parse_request({"Close", "42"}).request.command == Command::close);
    assert(!parse_request({}).ok && !parse_request({"Bogus"}).ok);

    const std::uint64_t largest = std::numeric_limits<std::uint64_t>::max();
    const std::string encoded = cp0::pty::encode_handle(largest);
    std::uint64_t decoded = 0;
    assert(cp0::pty::decode_handle(encoded, decoded) && decoded == largest);
    assert(cp0::pty::encode_handle(0).empty());
    assert(!cp0::pty::decode_handle("18446744073709551616", decoded));
    assert(decoded == 0);
    assert(!cp0::pty::decode_handle("42 ", decoded));
    return 0;
}
