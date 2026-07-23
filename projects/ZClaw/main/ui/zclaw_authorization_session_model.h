#pragma once

#include <cstdint>

namespace zclaw {

class AuthorizationSession {
public:
    using Generation = std::uint64_t;

    Generation begin();
    bool finish(Generation generation);
    void invalidate();

    bool in_flight() const;
    Generation generation() const;

private:
    Generation generation_ = 0;
    bool in_flight_ = false;
};

}  // namespace zclaw
