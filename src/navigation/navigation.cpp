#include "navigation/navigation.hpp"
#include <algorithm>

namespace spatial {
bool selection_matches(const SelectionKey& key,const Item& item) {
    return key.group==item.group && (key.group==GroupKind::None || key.label==item.label) &&
        std::find(item.members.begin(),item.members.end(),key.anchor)!=item.members.end();
}
void Navigation::select_item(std::size_t index,const Item& item) {
    if (item.members.empty()) { clear_selection(); return; }
    current_.selected=index; current_.selection=SelectionKey{item.group,item.members.front(),item.label};
}
std::vector<EntryId> Navigation::entries(const Filesystem& fs) const {
    return current_.group ? current_.group->members:fs.children(current_.directory);
}
std::string Navigation::path(const Filesystem& fs) const {
    const auto directory=fs.path(current_.directory);
    return current_.group ? directory+" / ["+current_.group->label+"]":directory;
}
void Navigation::select(std::size_t index,std::size_t count) {
    current_.selected=count ? std::min(index,count-1):no_selection;
    current_.selection.reset();
}
void Navigation::next(std::size_t count) { select(count ? (current_.selected+1)%count:0,count); }
void Navigation::page(int delta,std::size_t count) {
    if (!count) return;
    if (delta<0 && current_.page>0) --current_.page;
    if (delta>0 && current_.page+1<count) ++current_.page;
    current_.selected=0; current_.selection.reset();
}
bool Navigation::open(const Filesystem& fs,const Item& item) {
    if (item.members.empty()) return false;
    auto next=current_;
    if (item.group!=GroupKind::None) next.group=GroupView{item.label,item.members};
    else if (fs.entry(item.members.front()).kind==EntryKind::Directory) {
        next.directory=item.members.front(); next.group.reset();
    } else return false;
    parents_.push_back(current_);
    next.selected=0; next.page=0; next.selection.reset();
    current_=std::move(next);
    return true;
}
bool Navigation::up() {
    if (parents_.empty()) return false;
    current_=std::move(parents_.back()); parents_.pop_back(); return true;
}
}
