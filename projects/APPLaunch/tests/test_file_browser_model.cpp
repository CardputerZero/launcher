#include "../main/ui/model/file_browser_model.hpp"

#include <cassert>
#include <string>

int main()
{
    FileBrowserModel model;
    assert(model.current_path() == "/");
    assert(model.apply_listing(0, "F\t12\tz.txt\nD\t0\tFolder%20One\nF\t3\ta.txt\n"));
    assert(model.entries().size() == 3);
    assert(model.entries()[0].is_directory);
    assert(model.entries()[0].name == "Folder One");
    assert(model.entries()[1].name == "a.txt");
    assert(!model.listing_failed());
    assert(!model.select_previous());
    assert(model.enter_selected());
    assert(model.current_path() == "/Folder One");

    assert(model.apply_listing(
        0, "D\t0\tchild\nF\t9\tbad%2Fname\nD\t0\tbad%5Cname\nD\t0\t..\n"));
    assert(model.entries().size() == 1);
    assert(model.navigate_parent());
    assert(model.current_path() == "/");
    assert(!model.navigate_parent());

    assert(model.apply_listing(0, "F\t1\ta\nF\t2\tb\n"));
    assert(model.select_next());
    assert(model.selected_index() == 1);
    assert(!model.select_next());
    assert(!model.enter_selected());
    assert(!model.apply_listing(-1, "F\t1\tignored\n"));
    assert(model.entries().empty());
    assert(model.listing_failed());

    assert(model.apply_listing(
        0, "F\t9223372036854775808\toverflow\nF\t1\tbad\\name\nF\t7\tvalid\n"));
    assert(!model.listing_failed());
    assert(model.entries().size() == 1);
    assert(model.entries()[0].name == "valid");

    std::string oversized_listing;
    for (std::size_t index = 0; index < FileBrowserModel::MAX_ENTRIES + 2; ++index)
        oversized_listing += "F\t1\tfile" + std::to_string(index) + "\n";
    assert(model.apply_listing(0, oversized_listing));
    assert(model.entries().size() == FileBrowserModel::MAX_ENTRIES);
    return 0;
}
