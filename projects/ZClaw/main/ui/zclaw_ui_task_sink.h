#pragma once

#include <functional>

namespace zclaw {

class UiTaskSink {
public:
    using Task = std::function<void()>;

    virtual ~UiTaskSink() = default;
    virtual bool post(Task task) = 0;
};

}  // namespace zclaw
