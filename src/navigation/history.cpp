#include "navigation/history.hpp"
#include <algorithm>

namespace spatial {
void History::push(HistoryEntry entry) {
    if (!entries_.empty()) entries_.erase(entries_.begin()+static_cast<std::ptrdiff_t>(position_+1),entries_.end());
    entries_.push_back(std::move(entry));
    if (entries_.size()>64) entries_.erase(entries_.begin());
    position_=entries_.size()-1;
}
std::optional<std::size_t> History::target(int delta) const {
    if (entries_.empty()) return {};
    if (delta<0) for (auto i=position_;i>0;) { --i; if (entries_[i].valid) return i; }
    if (delta>0) for (auto i=position_+1;i<entries_.size();++i) if (entries_[i].valid) return i;
    return {};
}
void History::move_to(std::size_t index) { if (index<entries_.size()) position_=index; }
}
