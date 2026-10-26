#include "app/browser.hpp"
#include <algorithm>

namespace spatial {
std::optional<std::filesystem::path> Browser::surrounding_directory() const {
    if (!surroundings_ || surroundings_->active_node==no_node) return {};
    return surroundings_->filesystem.path(root_id);
}
void Browser::request_surroundings() {
    if (!loader_) return;
    if (world_path_ && world_path_->parent_path()==*world_path_) return;
    if (surroundings_) { schedule_surroundings(); return; }
    // Resolve on the existing worker: a starting symlink or '..' must use its
    // actual parent, without filesystem queries in presentation/layout code.
    for (const auto& [tag,action]:validations_) {
        (void)tag;
        if (action.purpose==Purpose::Surroundings && action.generation==generation_) return;
    }
    const auto tag=next_validation_++;
    validations_[tag]={Purpose::Surroundings,0,0,generation_};
    loader_->validate(tag,filesystem.path(root_id),true);
}
void Browser::schedule_surroundings() {
    if (!loader_ || !surroundings_) return;
    auto& context=*surroundings_;
    const auto request=[&](EntryId directory,std::size_t limit) {
        if (context.pending.insert(directory).second) {
            context.filesystem.mark_loading(directory);
            loader_->request(directory,context.filesystem.path(directory),limit,generation_,surrounding_scope);
        }
    };
    if (context.filesystem.state(root_id).state==LoadState::Unloaded) { request(root_id,0); return; }
    if (context.active_node==no_node) return;
    for (const auto& object:context.hierarchy.node(0).world.objects) {
        const auto& item=object.item;
        if (item.category!=Category::Directory || item.group!=GroupKind::None) continue;
        const auto directory=item.members.front();
        if (context.filesystem.path(directory)==context.active || context.filesystem.state(directory).state!=LoadState::Unloaded) continue;
        request(directory,std::max<std::size_t>(1,load_policy.preview_entries));
    }
}
void Browser::apply_surroundings(LoadResult result) {
    if (!surroundings_) return;
    auto& context=*surroundings_;
    context.pending.erase(result.directory);
    context.filesystem.apply(result.directory,std::move(result.listing));
    const auto& fs=context.filesystem;
    const auto& children=fs.children(result.directory);
    auto items=analyze(fs,children,now_);
    auto individuals=analyze(fs,children,now_,{},false);
    if (result.directory==root_id) {
        if (fs.state(root_id).state==LoadState::Error) { context.error=fs.state(root_id).message; return; }
        // Locate the active directory before choosing a page; even an old or
        // hidden CWD must appear individually rather than inside Other entries.
        auto active=std::find_if(items.begin(),items.end(),[&](const Item& item) {
            return item.group==GroupKind::None && item.category==Category::Directory &&
                std::filesystem::path(fs.path(item.members.front()))==context.active;
        });
        if (active==items.end()) { context.error="Current directory is absent from its parent listing"; return; }
        const auto offset=static_cast<std::size_t>(active-items.begin());
        context.hierarchy.initialize({{root_id,items}},std::move(individuals));
        const auto capacity=page_capacity(items.size(),context.hierarchy.layout_config);
        context.hierarchy.set_page(0,offset/capacity);
        const auto index=offset%capacity;
        context.active_node=context.hierarchy.child(0,index);
        // Frame the backdrop around the already-existing root. Its active child
        // becomes the identity transform, so no CWD objects or camera move when
        // the asynchronous parent/sibling data arrives.
        const auto anchor=contained_transform(context.hierarchy.node(0).local.objects[index]);
        const float scale=1/anchor.scale;
        context.hierarchy.transform_world({anchor.translation*(-scale),scale});
    } else if (context.active_node!=no_node) {
        context.hierarchy.upsert_directory({result.directory,std::move(items)},std::move(individuals));
    }
    schedule_surroundings();
}
void Browser::refresh_surroundings() {
    if (!surroundings_) return;
    auto& context=*surroundings_;
    if (context.active_node==no_node) return;
    std::vector<unsigned char> levels(context.hierarchy.size(),0);
    levels[0]=2; // All siblings on this bounded page; their contents stay compact.
    AppearanceMap appearance;
    auto& styles=appearance[0];
    for (const auto& object:context.hierarchy.node(0).world.objects) {
        ObjectAppearance style;
        if (object.item.category==Category::Directory && object.item.group==GroupKind::None) {
            const auto state=context.filesystem.state(object.item.members.front()).state;
            if (state==LoadState::Unloaded) style.contents_status=" [loading]";
            else if (state==LoadState::Error) style.contents_status=" [unavailable]";
        }
        styles.push_back(std::move(style));
    }
    // The main scene supplies the CWD and all its descendants, picking targets
    // and names. The backdrop supplies only surrounding geometry, without a
    // duplicate CWD enclosure or labels inside siblings.
    context.boxes=present(context.hierarchy,context.active_node,no_selection,levels,appearance,presentation_config,context.active_node).boxes;
}
}
