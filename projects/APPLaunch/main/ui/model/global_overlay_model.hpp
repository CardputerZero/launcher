#pragma once

class GlobalOverlayModel
{
public:
    bool request_show()
    {
        visible_ = true;
        return !created_;
    }

    void mark_created() { created_ = true; }
    void hide() { visible_ = false; }
    void object_deleted()
    {
        created_ = false;
        visible_ = false;
    }

    bool created() const { return created_; }
    bool visible() const { return visible_; }

private:
    bool created_ = false;
    bool visible_ = false;
};
