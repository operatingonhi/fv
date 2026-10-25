#pragma once
#include "layout/layout.hpp"
#include <limits>
#include <map>

namespace spatial {
using NodeId=std::size_t;
inline constexpr NodeId no_node=std::numeric_limits<NodeId>::max();
// Uniform parent-relative scale and translation are enough for planar dioramas.
struct LocalTransform {
    Vec3 translation{};
    float scale=1;
    Vec3 point(Vec3 local) const { return translation+local*scale; }
};
LocalTransform compose(LocalTransform parent,LocalTransform child);
inline constexpr Vec3 diorama_focus{0,0,-2.5f};
inline constexpr Vec3 diorama_size{16,2.5f,16};
LocalTransform contained_transform(const Placement& local);
struct DirectoryItems { EntryId directory; std::vector<Item> items; };
struct HierarchyConfig { unsigned preview_depth=3, max_depth=6; };
struct LayoutNode {
    NodeId parent=no_node;
    unsigned depth=0;
    LocalTransform transform;
    GroupKind group=GroupKind::None;
    std::vector<Item> items;
    Layout local, world;
    std::vector<NodeId> children;
    std::map<std::size_t,std::vector<NodeId>> children_by_page;
};
// Input is already analyzed ordinary data. No filesystem retrieval in this layer.
class Hierarchy {
public:
    void initialize(std::vector<DirectoryItems> directories,std::vector<Item> individuals);
    const LayoutNode& node(NodeId id) const { return nodes_.at(id); }
    NodeId child(NodeId id,std::size_t index) const;
    void ensure_preview(NodeId id,unsigned depth);
    void set_page(NodeId id,std::size_t page);
    const std::vector<NodeId>& visible_nodes() const { return visible_; }
    Vec3 focus(NodeId id) const { return node(id).transform.point(diorama_focus); }
    std::size_t size() const { return nodes_.size(); }
    HierarchyConfig config;
private:
    NodeId add(std::vector<Item> items,NodeId parent,LocalTransform transform,GroupKind group);
    void arrange_node(NodeId id,std::size_t page);
    void expand(NodeId id,unsigned depth);
    void collect(NodeId id);
    std::vector<DirectoryItems> directories_;
    std::vector<Item> individuals_;
    std::vector<LayoutNode> nodes_;
    std::vector<NodeId> visible_;
};
}
