#include "zclaw_authorization_session_model.h"

namespace zclaw {

AuthorizationSession::Generation AuthorizationSession::begin()
{
    ++generation_;
    in_flight_ = true;
    return generation_;
}

bool AuthorizationSession::finish(Generation generation)
{
    if (!in_flight_ || generation != generation_)
        return false;
    in_flight_ = false;
    return true;
}

void AuthorizationSession::invalidate()
{
    ++generation_;
    in_flight_ = false;
}

bool AuthorizationSession::in_flight() const
{
    return in_flight_;
}

AuthorizationSession::Generation AuthorizationSession::generation() const
{
    return generation_;
}

}  // namespace zclaw
