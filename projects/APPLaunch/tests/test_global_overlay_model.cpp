#include "../main/ui/model/global_overlay_model.hpp"

#include <cassert>

int main()
{
    GlobalOverlayModel model;
    assert(model.request_show());
    model.mark_created();
    assert(model.created() && model.visible());
    assert(!model.request_show());

    model.hide();
    model.hide();
    assert(model.created() && !model.visible());

    model.object_deleted();
    assert(!model.created() && !model.visible());
    assert(model.request_show());
    model.mark_created();
    assert(model.created() && model.visible());
}
