#include "app/browser.hpp"
#include <algorithm>

namespace spatial {
namespace {
CameraState in_frame(CameraState state,LocalTransform frame) {
    state.focus=frame.point(state.focus);
    state.distance*=frame.scale; state.scale*=frame.scale;
    return state;
}
LocalTransform inverse(LocalTransform frame) {
    const float scale=1/frame.scale;
    return {frame.translation*(-scale),scale};
}
}
void Browser::cancel_parent() {
    pending_parent_.reset();
    std::erase_if(validations_,[](const auto& entry) { return entry.second.purpose==Purpose::Parent; });
    if (location_tag_ && !validations_.contains(location_tag_)) location_tag_=0;
}
Browser::Surroundings Browser::cached_parent(EntryId directory) const {
    const auto parent=filesystem.entry(directory).parent;
    const auto path=[&](EntryId id) {
        return world_path_ ? *world_path_/std::filesystem::path(filesystem.path(id)).lexically_relative(filesystem.path(root_id)):
            std::filesystem::path(filesystem.path(id));
    };
    Surroundings context(path(parent),path(directory));
    const auto entries=filesystem.copy_subtree(parent,context.filesystem,root_id);
    context.hierarchy.layout_config=hierarchy.layout_config; context.hierarchy.config=hierarchy.config;
    context.hierarchy.initialize({{root_id,{}}},{});
    NodeId source=no_node;
    for (auto id:hierarchy.visible_nodes()) if (hierarchy.node(id).directory==parent) { source=id; break; }
    if (source!=no_node) hierarchy.copy_branch(source,context.hierarchy,0,entries);
    else {
        const auto& children=context.filesystem.children(root_id);
        context.hierarchy.upsert_directory({root_id,analyze(context.filesystem,children,now_)},
            analyze(context.filesystem,children,now_,{},false));
    }
    const auto target=entries.at(directory);
    context.active=context.filesystem.path(target);
    const auto& items=context.hierarchy.node(0).items;
    const auto found=std::find_if(items.begin(),items.end(),[&](const auto& item) {
        return item.group==GroupKind::None && item.members.front()==target;
    });
    const auto offset=static_cast<std::size_t>(found-items.begin());
    const auto capacity=page_capacity(items.size(),context.hierarchy.layout_config);
    context.hierarchy.set_page(0,offset/capacity);
    context.hierarchy.ensure_preview(0,context.hierarchy.config.preview_depth);
    context.active_node=context.hierarchy.child(0,offset%capacity);
    const auto anchor=contained_transform(context.hierarchy.node(0).local.objects[offset%capacity]);
    context.hierarchy.transform_world(inverse(anchor));
    return context;
}
void Browser::enter_rebased(NodeId child,EntryId directory,bool remember_parent) {
    const auto frame=hierarchy.node(child).transform;
    const auto incoming=in_frame(camera.state(),inverse(frame));
    const auto path=world_path_ ? *world_path_/std::filesystem::path(filesystem.path(directory)).lexically_relative(filesystem.path(root_id)):
        std::filesystem::path(filesystem.path(directory));
    Filesystem next_filesystem(path.string());
    const auto entries=filesystem.copy_subtree(directory,next_filesystem,root_id);
    Hierarchy next_hierarchy; next_hierarchy.layout_config=hierarchy.layout_config; next_hierarchy.config=hierarchy.config;
    next_hierarchy.initialize({{root_id,{}}},{});
    hierarchy.copy_branch(child,next_hierarchy,0,entries);
    next_hierarchy.ensure_preview(0,next_hierarchy.config.preview_depth);
    auto context=cached_parent(directory);
    const ParentReturn back{history.current().state,directory,frame};
    if (remember_parent) save_view();
    stash_world(); world_=std::make_shared<World>();
    filesystem=std::move(next_filesystem); hierarchy=std::move(next_hierarchy);
    surroundings_=std::move(context); if (remember_parent) parent_return_=back;
    world_path_=path; navigation=Navigation{}; current_=0; parents_.clear();
    navigation.restore_page(hierarchy.node(0).world.page);
    camera.restore(incoming,false); refresh_surroundings(); sync_view(); focus_overview();
    request_directory(0,true,true); schedule_previews(); record_view();
}
bool Browser::return_to_cached_parent() {
    if (!parent_return_ || !views_.contains(parent_return_->state)) return false;
    const auto back=*parent_return_;
    const auto child_world=world_;
    const auto incoming=camera.state();
    const auto query=query_;
    save_view(); restore_view(back.state);
    // The old world remains owned only by bounded history. Copy newly loaded
    // child data back before zooming out, retaining the child's local anchors.
    const auto entries=child_world->filesystem->copy_subtree(0,filesystem,back.directory);
    auto frame=back.frame;
    for (std::size_t i=0;i<layout.objects.size();++i) {
        const auto& item=layout.objects[i].item;
        if (item.group!=GroupKind::None || item.members.front()!=back.directory) continue;
        const auto child=hierarchy.child(current_,i);
        if (child!=no_node) {
            frame=hierarchy.node(child).transform;
            child_world->hierarchy->copy_branch(0,hierarchy,child,entries);
        }
        break;
    }
    query_=query; camera.restore(in_frame(incoming,frame),false);
    sync_view(); focus_overview(); schedule_previews(); record_view();
    return true;
}
void Browser::try_ascend() {
    if (!pending_parent_) return;
    if (world_path_ && *world_path_==*pending_parent_) { pending_parent_.reset(); notice="Already at filesystem root"; return; }
    if (!surroundings_) { request_surroundings(); return; }
    auto& context=*surroundings_;
    if (!context.error.empty()) { notice="Cannot prepare parent view: "+context.error; pending_parent_.reset(); return; }
    if (context.active_node==no_node) { schedule_surroundings(); return; }
    const auto path=*pending_parent_;
    Filesystem next_filesystem(path.string());
    const auto entries=context.filesystem.copy_subtree(0,next_filesystem,0);
    Hierarchy next_hierarchy; next_hierarchy.layout_config=hierarchy.layout_config; next_hierarchy.config=hierarchy.config;
    next_hierarchy.initialize({{0,{}}},{});
    const auto nodes=context.hierarchy.copy_branch(0,next_hierarchy,0,entries);
    const auto child=nodes.at(context.active_node);
    const auto directory=*next_hierarchy.node(child).directory;
    const auto current_entries=filesystem.copy_subtree(0,next_filesystem,directory);
    hierarchy.copy_branch(0,next_hierarchy,child,current_entries);
    const auto incoming=in_frame(camera.state(),next_hierarchy.node(child).transform);
    save_view(); stash_world(); world_=std::make_shared<World>();
    filesystem=std::move(next_filesystem); hierarchy=std::move(next_hierarchy);
    current_=0; parents_.clear(); navigation=Navigation{}; world_path_=path;
    navigation.restore_page(hierarchy.node(0).world.page);
    camera.restore(incoming,false); sync_view(); focus_overview();
    request_directory(0,true,true); schedule_previews(); request_surroundings(); record_view();
}
}
