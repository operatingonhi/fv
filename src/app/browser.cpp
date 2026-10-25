#include "app/browser.hpp"
#include <algorithm>
#include <chrono>

namespace spatial {
Browser::Browser():filesystem(make_mock_filesystem()) {
    std::vector<DirectoryItems> directories;
    std::vector<EntryId> all;
    for (EntryId id=0;id<filesystem.size();++id) {
        all.push_back(id);
        if (filesystem.entry(id).kind==EntryKind::Directory) {
            const auto children=filesystem.children(id);
            directories.push_back({id,analyze(filesystem,children,mock_now)});
        }
    }
    initialize(std::move(directories),analyze(filesystem,all,now_,{},false));
}
void Browser::initialize(std::vector<DirectoryItems> directories,std::vector<Item> individuals) {
    hierarchy.initialize(std::move(directories),std::move(individuals));
    camera.focus(hierarchy.focus(0),false,24);
    sync_view();
}
Browser::Browser(const std::filesystem::path& start):filesystem(start.string()),loader_(std::make_unique<DirectoryLoader>()) {
    now_=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    std::error_code ec;
    const auto absolute=std::filesystem::absolute(start,ec);
    if (!ec) filesystem=Filesystem(absolute.string());
    initialize({DirectoryItems{root_id,{}}},{});
    if (ec) {
        DirectoryListing failed; failed.error="Starting path: "+ec.message();
        filesystem.apply(root_id,std::move(failed)); update_status();
    } else request_directory(root_id,true);
}
void Browser::request_directory(EntryId id,bool full,bool explicit_request) {
    if (!loader_ || filesystem.entry(id).kind!=EntryKind::Directory) return;
    const auto state=filesystem.state(id).state;
    if (state==LoadState::Ready || (state==LoadState::Error && !explicit_request)) return;
    if (!full && state!=LoadState::Unloaded) return;
    const auto pending=pending_.find(id);
    if (pending!=pending_.end() && (!full || pending->second)) return;
    pending_[id]=full; filesystem.mark_loading(id);
    loader_->request(id,filesystem.path(id),full ? 0:std::max<std::size_t>(1,load_policy.preview_entries));
    update_status();
}
void Browser::schedule_previews() {
    if (!loader_) return;
    std::size_t directories=0;
    for (const auto& placement:layout.objects) {
        const auto& item=placement.item;
        if (item.group==GroupKind::None && item.category==Category::Directory && directories++<load_policy.preview_directories)
            request_directory(item.members.front(),false);
    }
}
void Browser::update_status() {
    if (!loader_) { status="Deterministic mock data"; return; }
    const auto& state=filesystem.state(navigation.context().directory);
    status=state.loading ? "Loading directory...":state.state==LoadState::Ready ? "Directory loaded":
        state.state==LoadState::Preview ? "Partial preview; loading fully on entry":
        state.state==LoadState::Error ? "Directory unavailable":"Directory not loaded";
    if (!state.message.empty()) status+=" | "+state.message;
    if (!pending_.empty()) status+=" | "+std::to_string(pending_.size())+" loads/updates pending";
    if (!deferred_.empty()) status+=" | Return to parent to apply ready updates";
}
void Browser::refresh_scene() {
    detail_levels(hierarchy,camera,levels_);
    scene=present(hierarchy,current_,navigation.selected(),levels_);
    ++scene_revision;
}
void Browser::sync_view() {
    layout=hierarchy.node(current_).world;
    navigation.select(navigation.selected(),layout.objects.size());
    if (navigation.context().group) {
        std::vector<EntryId> members;
        for (const auto& item:hierarchy.node(current_).items)
            members.insert(members.end(),item.members.begin(),item.members.end());
        navigation.update_group(std::move(members));
    }
    refresh_scene(); update_status();
}
void Browser::focus_overview() {
    const auto scale=hierarchy.node(current_).transform.scale;
    camera.focus(hierarchy.focus(current_),animations,24*scale,scale);
}
void Browser::update(float seconds) {
    bool changed=false;
    std::vector<LoadResult> results;
    if (loader_) results=loader_->poll();
    if (apply_deferred_) {
        for (auto& result:deferred_) results.push_back(std::move(result));
        deferred_.clear(); apply_deferred_=false;
    }
    bool status_changed=false;
    for (auto& result:results) {
        // Don't regroup an enclosing directory while the user is inside one of
        // its groups/descendants: that could detach the active diorama. Apply the
        // ready snapshot on return, retaining the old readable context meanwhile.
        bool enclosing=false;
        for (auto id=navigation.context().directory;;id=filesystem.entry(id).parent) {
            if (id==result.directory) { enclosing=true; break; }
            if (id==root_id) break;
        }
        if (enclosing && (navigation.context().directory!=result.directory || navigation.context().group)) {
            deferred_.push_back(std::move(result)); status_changed=true; continue;
        }
        const auto pending=pending_.find(result.directory);
        const bool full_pending=pending!=pending_.end() && pending->second;
        if (result.limit==0 || !full_pending) pending_.erase(result.directory);
        // Never let an older partial response downgrade a completed directory.
        if (filesystem.state(result.directory).state==LoadState::Ready && !result.listing.complete) continue;
        filesystem.apply(result.directory,std::move(result.listing));
        if (pending_.contains(result.directory)) filesystem.mark_loading(result.directory);
        const auto& children=filesystem.children(result.directory);
        hierarchy.upsert_directory({result.directory,analyze(filesystem,children,now_)},analyze(filesystem,children,now_,{},false));
        changed=true;
    }
    if (changed) {
        hierarchy.ensure_preview(current_,hierarchy.config.preview_depth);
        sync_view(); schedule_previews(); update_status();
    }
    if (status_changed) update_status();
    camera.update(seconds);
    detail_levels(hierarchy,camera,scratch_levels_);
    if (scratch_levels_!=levels_) refresh_scene();
}
void Browser::act(UiAction action) {
    switch (action.kind) {
    case ActionKind::None: return;
    case ActionKind::Focus:
    case ActionKind::Open: {
        if (action.index>=layout.objects.size()) return;
        const bool selection_changed=navigation.selected()!=action.index;
        navigation.select(action.index,layout.objects.size());
        const auto child=hierarchy.child(current_,action.index);
        if (action.kind==ActionKind::Open && child!=no_node &&
            navigation.open(filesystem,layout.objects[action.index].item)) {
            parents_.push_back(current_); current_=child;
            navigation.restore_page(hierarchy.node(current_).world.page);
            hierarchy.ensure_preview(current_,hierarchy.config.preview_depth);
            sync_view(); focus_overview();
            request_directory(navigation.context().directory,true,true); schedule_previews();
        } else {
            const auto scale=hierarchy.node(current_).transform.scale;
            camera.focus(layout.objects[action.index].center,animations,13*scale,scale);
            if (selection_changed) refresh_scene();
            const auto& item=layout.objects[action.index].item;
            if (item.group==GroupKind::None && item.category==Category::Directory) request_directory(item.members.front(),false);
        }
        break;
    }
    case ActionKind::Up:
        if (navigation.up()) {
            apply_deferred_=true;
            current_=parents_.back(); parents_.pop_back();
            sync_view(); focus_overview(); schedule_previews();
        }
        break;
    case ActionKind::PreviousPage:
    case ActionKind::NextPage:
        if ((action.kind==ActionKind::PreviousPage && layout.page==0) ||
            (action.kind==ActionKind::NextPage && layout.page+1>=layout.pages)) return;
        navigation.page(action.kind==ActionKind::PreviousPage ? -1:1,layout.pages);
        hierarchy.set_page(current_,navigation.context().page);
        sync_view(); focus_overview(); schedule_previews();
        break;
    case ActionKind::Reset: focus_overview(); break;
    }
}
}
