#include "zclaw_authorization_session_model.h"

#include <cassert>

int main()
{
    zclaw::AuthorizationSession session;
    assert(!session.in_flight());
    assert(session.generation() == 0);

    const zclaw::AuthorizationSession::Generation first = session.begin();
    assert(first != 0);
    assert(session.in_flight());
    assert(session.generation() == first);

    const zclaw::AuthorizationSession::Generation second = session.begin();
    assert(second != first);
    assert(session.in_flight());
    assert(!session.finish(first));
    assert(session.in_flight());
    assert(session.finish(second));
    assert(!session.in_flight());
    assert(!session.finish(second));

    const zclaw::AuthorizationSession::Generation cleared = session.begin();
    session.invalidate();
    assert(!session.in_flight());
    assert(session.generation() != cleared);
    assert(!session.finish(cleared));

    const zclaw::AuthorizationSession::Generation failed = session.begin();
    assert(session.finish(failed));
    assert(!session.in_flight());

    const zclaw::AuthorizationSession::Generation current = session.begin();
    assert(!session.finish(failed));
    assert(session.in_flight());
    assert(session.finish(current));
    return 0;
}
