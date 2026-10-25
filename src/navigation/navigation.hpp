#pragma once
#include "analysis/analysis.hpp"
#include <optional>

namespace spatial {
struct GroupView { std::string label; std::vector<EntryId> members; };
struct NavigationContext {
    EntryId directory=root_id;
    std::optional<GroupView> group;
    std::size_t selected=0, page=0;
};
// Semantic state only: no layout coordinates, camera, ImGui, or OpenGL.
class Navigation {
public:
    const NavigationContext& context() const { return current_; }
    std::size_t selected() const { return current_.selected; }
    std::vector<EntryId> entries(const Filesystem& fs) const;
    std::string path(const Filesystem& fs) const;
    void select(std::size_t index,std::size_t count);
    void next(std::size_t count);
    void page(int delta,std::size_t count);
    void restore_page(std::size_t page) { current_.page=page; current_.selected=0; }
    bool open(const Filesystem& fs,const Item& item);
    bool up();
    void update_group(std::vector<EntryId> members) { if (current_.group) current_.group->members=std::move(members); }
    bool can_go_up() const { return !parents_.empty(); }
private:
    NavigationContext current_;
    std::vector<NavigationContext> parents_;
};
}
