#pragma once
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace spatial {
struct HistoryEntry { std::string directory; std::size_t state; bool valid=true; };
// Opaque state handles keep camera/layout objects out of navigation history.
class History {
public:
    void push(HistoryEntry entry);
    std::optional<std::size_t> target(int delta) const;
    void move_to(std::size_t index);
    void invalidate(std::size_t index) { if (index<entries_.size()) entries_[index].valid=false; }
    const HistoryEntry& at(std::size_t index) const { return entries_.at(index); }
    const HistoryEntry& current() const { return at(position_); }
    const std::vector<HistoryEntry>& entries() const { return entries_; }
    std::size_t position() const { return position_; }
    bool can_back() const { return target(-1).has_value(); }
    bool can_forward() const { return target(1).has_value(); }
private:
    std::vector<HistoryEntry> entries_;
    std::size_t position_=0;
};
}
