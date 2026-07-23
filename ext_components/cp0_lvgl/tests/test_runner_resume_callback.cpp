#include "cp0_runner_contract.hpp"

#include <cassert>
#include <stdexcept>

int main()
{
    int resumes = 0;
    cp0::invoke_resume_callback([&] { ++resumes; });
    assert(resumes == 1);

    cp0::invoke_resume_callback([&] {
        ++resumes;
        throw std::runtime_error("mutex lock");
    });
    assert(resumes == 2);
}
