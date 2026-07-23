#include "ip_panel_model.hpp"

#include <algorithm>

void IpPanelModel::replace(std::vector<IpInterfaceInfo> interfaces)
{
    const std::string selected_name = selected_index_ < interfaces_.size()
        ? interfaces_[selected_index_].name
        : std::string();
    std::sort(interfaces.begin(), interfaces.end(),
              [](const IpInterfaceInfo &left, const IpInterfaceInfo &right) {
                  return left.name < right.name;
              });
    interfaces_ = std::move(interfaces);

    const auto selected = std::find_if(interfaces_.begin(), interfaces_.end(),
                                       [&](const IpInterfaceInfo &interface) {
                                           return interface.name == selected_name;
                                       });
    if (selected != interfaces_.end()) {
        selected_index_ = static_cast<size_t>(selected - interfaces_.begin());
    } else if (interfaces_.empty()) {
        selected_index_ = 0;
    } else if (selected_index_ >= interfaces_.size()) {
        selected_index_ = interfaces_.size() - 1;
    }
}

bool IpPanelModel::apply_refresh(bool loaded, std::vector<IpInterfaceInfo> interfaces)
{
    if (!loaded) return false;
    replace(std::move(interfaces));
    return true;
}

bool IpPanelModel::select_previous()
{
    if (selected_index_ == 0) return false;
    --selected_index_;
    return true;
}

bool IpPanelModel::select_next()
{
    if (selected_index_ + 1 >= interfaces_.size()) return false;
    ++selected_index_;
    return true;
}

size_t IpPanelModel::visible_offset(size_t visible_count) const
{
    if (visible_count == 0 || interfaces_.size() <= visible_count) return 0;
    const size_t centered = selected_index_ > visible_count / 2
        ? selected_index_ - visible_count / 2
        : 0;
    return std::min(centered, interfaces_.size() - visible_count);
}
