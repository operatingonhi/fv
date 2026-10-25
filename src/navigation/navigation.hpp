#pragma once
#include "analysis/analysis.hpp"
#include <optional>
#include <limits>

namespace spatial {
struct GroupView { std::string label; std::vector<EntryId> members; };
inline constexpr std::size_t no_selection=std::numeric_limits<std::size_t>::max();
struct SelectionKey {
    GroupKind group=GroupKind::None;
    EntryId anchor=0;
    std::string label;
};
bool selection_matches(const SelectionKey& key,const Item& item);
struct NavigationContext {
    EntryId directory=root_id;
    std::optional<GroupView> group;
    std::size_t selected=0, page=0;
    std::optional<SelectionKey> selection;
};
struct NavigationSnapshot { NavigationContext current; std::vector<NavigationContext> parents; };
// Semantic state only: no layout coordinates, camera, ImGui, or OpenGL.
class Navigation {
public:
    const NavigationContext& context() const { return current_; }
    std::size_t selected() const { return current_.selected; }
    std::vector<EntryId> entries(const Filesystem& fs) const;
    std::string path(const Filesystem& fs) const;
    void select(std::size_t index,std::size_t count);
    void select_item(std::size_t index,const Item& item);
    void clear_selection() { current_.selected=no_selection; current_.selection.reset(); }
    NavigationSnapshot snapshot() const { return {current_,parents_}; }
    void restore(NavigationSnapshot state) { current_=std::move(state.current); parents_=std::move(state.parents); }
    void next(std::size_t count);
    void page(int delta,std::size_t count);
    void restore_page(std::size_t page) { current_.page=page; current_.selected=0; current_.selection.reset(); }
    bool open(const Filesystem& fs,const Item& item);
    bool up();
    void update_group(std::vector<EntryId> members) { if (current_.group) current_.group->members=std::move(members); }
    bool can_go_up() const { return !parents_.empty(); }
private:
    NavigationContext current_;
    std::vector<NavigationContext> parents_;
};
}
