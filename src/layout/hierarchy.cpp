#include "layout/hierarchy.hpp"
#include <algorithm>
#include <stdexcept>

namespace spatial {
LocalTransform compose(LocalTransform parent,LocalTransform child) {
    return {parent.point(child.translation),parent.scale*child.scale};
}
LocalTransform contained_transform(const Placement& local) {
    const float scale=std::min(local.size.x/diorama_size.x,local.size.z/diorama_size.z)*0.94f;
    const Vec3 floor{local.center.x,local.center.y-local.size.y/2+0.025f,local.center.z};
    return {floor-diorama_focus*scale,scale};
}
void Hierarchy::initialize(std::vector<DirectoryItems> directories,std::vector<Item> individuals) {
    directories_=std::move(directories); individuals_=std::move(individuals);
    nodes_.clear(); visible_.clear();
    const auto root=std::find_if(directories_.begin(),directories_.end(),[](const auto& d) { return d.directory==root_id; });
    if (root==directories_.end()) throw std::invalid_argument("Missing analyzed mock root");
    add(root->items,no_node,{},GroupKind::None);
    ensure_preview(0,config.preview_depth);
}
NodeId Hierarchy::add(std::vector<Item> items,NodeId parent,LocalTransform transform,GroupKind group) {
    const NodeId id=nodes_.size();
    LayoutNode node;
    node.items=std::move(items); node.parent=parent; node.transform=transform; node.group=group;
    node.depth=parent==no_node ? 0:nodes_[parent].depth+1;
    nodes_.push_back(std::move(node));
    arrange_node(id,0);
    return id;
}
void Hierarchy::arrange_node(NodeId id,std::size_t page) {
    auto& node=nodes_[id];
    node.local=arrange(node.items,page);
    node.world=node.local;
    node.world.origin=node.transform.translation;
    for (auto& placement:node.world.objects) {
        placement.center=node.transform.point(placement.center);
        placement.size=placement.size*node.transform.scale;
    }
    auto& cached=node.children_by_page[node.local.page];
    if (cached.size()!=node.world.objects.size()) cached.assign(node.world.objects.size(),no_node);
    node.children=cached;
}
NodeId Hierarchy::child(NodeId id,std::size_t index) const {
    const auto& children=node(id).children;
    return index<children.size() ? children[index]:no_node;
}
void Hierarchy::expand(NodeId id,unsigned depth) {
    if (depth==0 || nodes_[id].depth>=config.max_depth) return;
    const auto count=nodes_[id].local.objects.size();
    for (std::size_t i=0;i<count;++i) {
        auto child_id=nodes_[id].children[i];
        if (child_id==no_node) {
            const auto placement=nodes_[id].local.objects[i];
            const auto& item=placement.item;
            std::vector<Item> items;
            if (item.group!=GroupKind::None) {
                for (auto member:item.members) {
                    const auto single=std::find_if(individuals_.begin(),individuals_.end(),[&](const Item& s) {
                        return s.members.front()==member;
                    });
                    if (single!=individuals_.end()) items.push_back(*single);
                }
            } else if (item.category==Category::Directory && !item.members.empty()) {
                const auto dir=std::find_if(directories_.begin(),directories_.end(),[&](const auto& d) {
                    return d.directory==item.members.front();
                });
                if (dir!=directories_.end()) items=dir->items;
            } else continue;
            const auto transform=compose(nodes_[id].transform,contained_transform(placement));
            child_id=add(std::move(items),id,transform,item.group);
            nodes_[id].children[i]=child_id;
            nodes_[id].children_by_page[nodes_[id].local.page][i]=child_id;
        }
        expand(child_id,depth-1);
    }
}
void Hierarchy::collect(NodeId id) {
    visible_.push_back(id);
    for (auto child_id:nodes_[id].children) if (child_id!=no_node) collect(child_id);
}
void Hierarchy::ensure_preview(NodeId id,unsigned depth) {
    expand(id,depth);
    visible_.clear(); collect(0);
}
void Hierarchy::set_page(NodeId id,std::size_t page) {
    if (page==nodes_[id].local.page) return;
    arrange_node(id,page);
    ensure_preview(id,config.preview_depth);
}
}
