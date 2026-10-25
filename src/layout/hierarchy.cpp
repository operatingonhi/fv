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
    directories_.clear(); individuals_.clear();
    for (auto& directory:directories) directories_[directory.directory]=std::move(directory.items);
    for (auto& item:individuals) individuals_[item.members.front()]=std::move(item);
    nodes_.clear(); visible_.clear();
    if (!directories_.contains(root_id)) throw std::invalid_argument("Missing analyzed root");
    add(directories_.at(root_id),no_node,{},GroupKind::None,root_id);
    ensure_preview(0,config.preview_depth);
}
NodeId Hierarchy::add(std::vector<Item> items,NodeId parent,LocalTransform transform,GroupKind group,std::optional<EntryId> directory) {
    const NodeId id=nodes_.size();
    LayoutNode node;
    node.items=std::move(items); node.parent=parent; node.transform=transform; node.group=group; node.directory=directory;
    node.depth=parent==no_node ? 0:nodes_[parent].depth+1;
    nodes_.push_back(std::move(node));
    arrange_node(id,0);
    return id;
}
void Hierarchy::arrange_node(NodeId id,std::size_t page) {
    auto& node=nodes_[id];
    auto config=layout_config;
    config.rank_remainder=node.group!=GroupKind::Series;
    node.local=arrange(node.items,page,{},config);
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
                    const auto single=individuals_.find(member);
                    if (single!=individuals_.end()) items.push_back(single->second);
                }
            } else if (item.category==Category::Directory && !item.members.empty()) {
                const auto dir=directories_.find(item.members.front());
                if (dir!=directories_.end()) items=dir->second;
            } else continue;
            const auto transform=compose(nodes_[id].transform,contained_transform(placement));
            child_id=add(std::move(items),id,transform,item.group,
                item.group==GroupKind::None ? std::optional<EntryId>{item.members.front()}:std::nullopt);
            nodes_[id].children[i]=child_id;
            nodes_[id].children_by_page[nodes_[id].local.page][i]=child_id;
        }
        expand(child_id,depth-1);
    }
}
namespace {
bool same_item(const Item& a,const Item& b) {
    if (a.group!=b.group) return false;
    if (a.group==GroupKind::None) return a.members==b.members;
    if (a.label!=b.label || a.members.empty() || b.members.empty()) return false;
    // Disjoint runs can share one filename pattern. Match by persistent members,
    // not label alone, while permitting an existing run to grow on full loading.
    return std::find(a.members.begin(),a.members.end(),b.members.front())!=a.members.end() ||
        std::find(b.members.begin(),b.members.end(),a.members.front())!=b.members.end();
}
}
void Hierarchy::replace_contents(NodeId id,std::vector<Item> items,bool preserve_order) {
    const auto old_layout=nodes_[id].local;
    const auto old_children=nodes_[id].children;
    if (preserve_order) {
        std::vector<Item> ordered;
        for (const auto& old:nodes_[id].items) {
            auto match=std::find_if(items.begin(),items.end(),[&](const Item& item) { return same_item(old,item); });
            if (match!=items.end()) { ordered.push_back(std::move(*match)); items.erase(match); }
        }
        ordered.insert(ordered.end(),std::make_move_iterator(items.begin()),std::make_move_iterator(items.end()));
        items=std::move(ordered);
    }
    nodes_[id].items=std::move(items);
    nodes_[id].children_by_page.clear();
    arrange_node(id,old_layout.page);
    auto& node=nodes_[id];
    std::vector<bool> pinned(node.local.objects.size(),false), old_pinned(old_layout.objects.size(),false);
    for (std::size_t i=0;i<node.local.objects.size();++i) {
        for (std::size_t j=0;j<old_layout.objects.size();++j) {
            if (old_pinned[j] || !same_item(node.local.objects[i].item,old_layout.objects[j].item)) continue;
            node.local.objects[i].center=old_layout.objects[j].center;
            node.local.objects[i].size=old_layout.objects[j].size;
            node.children[i]=old_children[j]; pinned[i]=true; old_pinned[j]=true; break;
        }
    }
    // Newly disclosed objects take unused grid slots rather than displacing known ones.
    for (std::size_t i=0;i<node.local.objects.size();++i) if (!pinned[i]) {
        for (std::size_t slot=0;slot<=std::max<std::size_t>(1,layout_config.page_size);++slot) {
            const auto position=layout_slot(slot,layout_config);
            const auto x=position.x,z=position.z;
            bool used=false;
            for (std::size_t j=0;j<node.local.objects.size();++j)
                if (pinned[j] && node.local.objects[j].center.x==x && node.local.objects[j].center.z==z) used=true;
            if (!used) {
                node.local.objects[i].center={x,node.local.objects[i].size.y/2,z}; pinned[i]=true; break;
            }
        }
    }
    prioritize_disclosure(node.local);
    node.world=node.local; node.world.origin=node.transform.translation;
    for (auto& placement:node.world.objects) {
        placement.center=node.transform.point(placement.center); placement.size=placement.size*node.transform.scale;
    }
    node.children_by_page[node.local.page]=node.children;
    // A growing compound retains its enclosure and updates its actual member units.
    const auto children=node.children;
    const auto objects=node.local.objects;
    for (std::size_t i=0;i<children.size();++i) if (children[i]!=no_node && objects[i].item.group!=GroupKind::None) {
        std::vector<Item> members;
        for (auto member:objects[i].item.members) if (individuals_.contains(member)) members.push_back(individuals_.at(member));
        replace_contents(children[i],std::move(members),false);
    }
}
void Hierarchy::upsert_directory(DirectoryItems directory,std::vector<Item> individuals) {
    for (auto& item:individuals) individuals_[item.members.front()]=std::move(item);
    directories_[directory.directory]=directory.items;
    const auto count=nodes_.size();
    for (NodeId id=0;id<count;++id)
        if (nodes_[id].directory==directory.directory) replace_contents(id,directory.items,true);
    ensure_preview(0,config.preview_depth);
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
