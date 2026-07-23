#include "cp0_status_component_contract.hpp"

#include <cassert>

int main()
{
    int first = 1;
    int second = 2;
    int *complete[] = {&first, &second};
    int *partial[] = {&first, nullptr};
    int *single[] = {&first};

    assert(cp0::status::resources_ready<int *>(nullptr, 0));
    assert(!cp0::status::resources_ready<int *>(nullptr, 1));
    assert(cp0::status::resources_ready(single, 1));
    single[0] = nullptr;
    assert(!cp0::status::resources_ready(single, 1));
    assert(cp0::status::resources_ready(complete, 2));
    assert(!cp0::status::resources_ready(partial, 2));
}
