#include "developer_page_model.hpp"

#include <algorithm>

namespace setting {
namespace {

std::size_t utf8_character_start(const std::string &text)
{
    std::size_t pos = text.size() - 1;
    while (pos > 0 && (static_cast<unsigned char>(text[pos]) & 0xC0U) == 0x80U) --pos;
    return pos;
}

std::size_t preview_start(const std::string &text, std::size_t byte_limit)
{
    if (text.size() <= byte_limit) return 0;
    std::size_t pos = text.size() - byte_limit;
    while (pos < text.size() && (static_cast<unsigned char>(text[pos]) & 0xC0U) == 0x80U) ++pos;
    return pos;
}

} // namespace

AdbPersistenceResult adb_persistence_result(bool set_succeeded,
                                            bool save_succeeded,
                                            bool rollback_set_succeeded,
                                            bool rollback_save_succeeded)
{
    if (!set_succeeded) return AdbPersistenceResult::SET_FAILED;
    if (save_succeeded) return AdbPersistenceResult::SAVED;
    if (rollback_set_succeeded && rollback_save_succeeded)
        return AdbPersistenceResult::SAVE_FAILED_ROLLED_BACK;
    return AdbPersistenceResult::SAVE_FAILED_ROLLBACK_FAILED;
}

const char *adb_persistence_error_detail(AdbPersistenceResult result)
{
    switch (result) {
    case AdbPersistenceResult::NOT_REQUESTED:
        return "ADB changed, but preference UI is unavailable";
    case AdbPersistenceResult::SET_FAILED:
        return "ADB changed, but preference write failed";
    case AdbPersistenceResult::SAVE_FAILED_ROLLED_BACK:
        return "ADB changed, but preference was not saved";
    case AdbPersistenceResult::SAVE_FAILED_ROLLBACK_FAILED:
        return "ADB changed; preference state is uncertain";
    default:
        return "";
    }
}

bool adb_visible_state_after_persistence(AdbPersistenceResult result,
                                         bool desired,
                                         bool previous)
{
    return result == AdbPersistenceResult::SAVED ? desired : previous;
}

bool DeveloperOverlayLifecycleModel::open()
{
    if (active_) return false;
    active_ = true;
    return true;
}

bool DeveloperOverlayLifecycleModel::close()
{
    if (!active_) return false;
    active_ = false;
    return true;
}

void DeveloperPageModel::begin_pairing(bool enable_after_pair)
{
    pair_key_.clear();
    enable_after_pair_ = enable_after_pair;
}

void DeveloperPageModel::clear_pairing()
{
    pair_key_.clear();
    enable_after_pair_ = false;
}

bool DeveloperPageModel::append_pairing_text(std::string_view text)
{
    if (text.empty() || text.size() > MAX_PAIR_KEY_BYTES - pair_key_.size()) return false;
    pair_key_.append(text.data(), text.size());
    return true;
}

bool DeveloperPageModel::erase_pairing_character()
{
    if (pair_key_.empty()) return false;
    pair_key_.erase(utf8_character_start(pair_key_));
    return true;
}

std::string DeveloperPageModel::pairing_preview() const
{
    const std::size_t start = preview_start(pair_key_, PAIR_PREVIEW_BYTES);
    return (start == 0 ? std::string{} : std::string{"..."}) + pair_key_.substr(start) + "_";
}

void DeveloperPageModel::reset_authorization_selection(std::size_t count)
{
    authorization_selection_ = 0;
    reconcile_authorization_count(count);
}

void DeveloperPageModel::reconcile_authorization_count(std::size_t count)
{
    if (count == 0)
        authorization_selection_ = 0;
    else
        authorization_selection_ = std::min(authorization_selection_, count - 1);
}

bool DeveloperPageModel::select_previous_authorization()
{
    if (authorization_selection_ == 0) return false;
    --authorization_selection_;
    return true;
}

bool DeveloperPageModel::select_next_authorization(std::size_t count)
{
    if (authorization_selection_ + 1 >= count) return false;
    ++authorization_selection_;
    return true;
}

std::size_t DeveloperPageModel::authorization_window_start(std::size_t visible_rows) const
{
    if (visible_rows == 0) return authorization_selection_;
    const std::size_t rows_before_selection = visible_rows / 2;
    return authorization_selection_ > rows_before_selection
               ? authorization_selection_ - rows_before_selection
               : 0;
}

} // namespace setting
