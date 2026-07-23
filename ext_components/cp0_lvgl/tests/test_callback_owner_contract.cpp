#include "cp0_callback_contract.hpp"

#include <cassert>
#include <stdexcept>

namespace {

struct Owner
{
    int updates = 0;
};

} // namespace

int main()
{
    Owner owner;
    cp0::callback::invoke_if_present(
        &owner, [](Owner &current) { ++current.updates; });
    assert(owner.updates == 1);

    cp0::callback::invoke_if_present(
        static_cast<Owner *>(nullptr),
        [](Owner &) { throw std::runtime_error("must not run"); });
    assert(owner.updates == 1);

    cp0::callback::invoke_if_present(
        &owner, [](Owner &) { throw std::runtime_error("status update"); });
    assert(owner.updates == 1);
}
