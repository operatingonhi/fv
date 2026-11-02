#include "app/browser.hpp"
#include <algorithm>
#include <chrono>
#include <set>
#include <cstdlib>
#include "navigation/path.hpp"
#include "graphics/picking.hpp"

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
    sync_view(); record_view();
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
    } else { request_directory(root_id,true); request_surroundings(); }
}
void Browser::request_directory(EntryId id,bool full,bool explicit_request) {
    if (!loader_ || filesystem.entry(id).kind!=EntryKind::Directory) return;
    const auto state=filesystem.state(id).state;
    if (state==LoadState::Ready || (state==LoadState::Error && !explicit_request)) return;
    if (!full && state!=LoadState::Unloaded) return;
    const auto pending=pending_.find(id);
    if (pending!=pending_.end() && (!full || pending->second)) return;
    pending_[id]=full; filesystem.mark_loading(id);
    loader_->request(id,filesystem.path(id),full ? 0:std::max<std::size_t>(1,load_policy.preview_entries),generation_);
    update_status();
}
void Browser::schedule_previews() {
    if (!loader_) return;
    for (const auto& placement:layout.objects) {
        const auto& item=placement.item;
        if (item.group==GroupKind::None && item.category==Category::Directory)
            request_directory(item.members.front(),true);
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
    if (surroundings_ && !surroundings_->error.empty()) status+=" | Parent context: "+surroundings_->error;
}
void Browser::refresh_scene() {
    const auto started=std::chrono::steady_clock::now();
    if (query_dirty_) { rebuild_query(); reconcile_selection(); }
    detail_levels(hierarchy,camera,levels_);
    last_lod_camera_=camera.state(); ++lod_evaluations;
    scene=present(hierarchy,current_,navigation.selected(),levels_,appearances_,presentation_config);
    scene.grid=grid_;
    if (current_==0 && surroundings_) scene.boxes.insert(scene.boxes.begin(),surroundings_->boxes.begin(),surroundings_->boxes.end());
    const auto& directory=filesystem.entry(navigation.context().directory);
    // Before metadata arrives, '.' and '..' are path syntax rather than names.
    // A path fallback also keeps the filesystem root visible (filename is empty).
    scene.context_name=display_name(directory.name.empty() || directory.name=="." || directory.name==".." ?
        directory.full_path:directory.name);
    if (navigation.context().group) scene.context_name+=" / ["+display_name(navigation.context().group->label)+"]";
    scene.labels.push_back(backdrop_inscription(hierarchy.node(current_).transform,scene.context_name,semantic_color(Category::Directory)));
    preparation_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
    ++scene_revision;
}
void Browser::sync_view() {
    layout=hierarchy.node(current_).world;
    if (navigation.context().group) {
        std::vector<EntryId> members;
        for (const auto& item:hierarchy.node(current_).items)
            members.insert(members.end(),item.members.begin(),item.members.end());
        navigation.update_group(std::move(members));
    }
    query_dirty_=true; refresh_scene(); update_status();
}
void Browser::focus_overview() {
    const auto scale=hierarchy.node(current_).transform.scale;
    camera.focus(hierarchy.focus(current_),animations,24*scale,scale,hierarchy.node(current_).transform.translation.y);
}
void Browser::update(float seconds) {
    for (const auto& error:launcher_.poll()) notice=desktop_launch_error_=error;
    if (loader_) for (auto& result:loader_->poll_validations()) process_validation(std::move(result));
    bool changed=false;
    std::vector<LoadResult> results;
    if (loader_) results=loader_->poll();
    if (apply_deferred_) {
        for (auto& result:deferred_) results.push_back(std::move(result));
        deferred_.clear(); apply_deferred_=false;
    }
    bool status_changed=false;
    bool surroundings_changed=false;
    std::set<EntryId> rescore_parents;
    for (auto& result:results) {
        if (result.generation!=generation_) continue;
        if (result.scope==surrounding_scope) { apply_surroundings(std::move(result)); surroundings_changed=true; continue; }
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
        if (result.directory!=root_id) rescore_parents.insert(filesystem.entry(result.directory).parent);
        changed=true;
    }
    for (auto parent:rescore_parents) {
        const auto& children=filesystem.children(parent);
        hierarchy.upsert_directory({parent,analyze(filesystem,children,now_)},analyze(filesystem,children,now_,{},false));
    }
    if (surroundings_changed) refresh_surroundings();
    if (changed) {
        hierarchy.ensure_preview(current_,hierarchy.config.preview_depth);
        sync_view(); schedule_previews(); update_status();
    }
    if (status_changed) update_status();
    if (surroundings_changed) { if (!changed) refresh_scene(); update_status(); }
    if (pending_parent_) try_ascend();
    camera.update(seconds);
    if (camera.state()!=last_lod_camera_) {
        detail_levels(hierarchy,camera,scratch_levels_,levels_);
        last_lod_camera_=camera.state(); ++lod_evaluations;
        if (scratch_levels_!=levels_) refresh_scene();
    }
}
const Item* Browser::selected_item() const {
    return navigation.selected()<layout.objects.size() ? &layout.objects[navigation.selected()].item:nullptr;
}
void Browser::rebuild_query() {
    query_result_=match_cached(filesystem,navigation.entries(filesystem),query_);
    matches_.clear();
    const auto& items=hierarchy.node(current_).items;
    for (std::size_t i=0;i<items.size();++i) if (eligible(items[i])) matches_.push_back(i);
    appearances_.clear();
    for (auto id:hierarchy.visible_nodes()) {
        auto& styles=appearances_[id];
        for (const auto& placement:hierarchy.node(id).world.objects) {
            const bool match=!query_.active() || item_matches(placement.item,query_result_);
            styles.push_back({match ? 1.0f:0.16f,match,query_.active() && match});
            if (loader_ && placement.item.group==GroupKind::None && placement.item.category==Category::Directory) {
                const auto& state=filesystem.state(placement.item.members.front());
                if (state.state==LoadState::Error) styles.back().contents_status=" [unavailable]";
                else if (state.state!=LoadState::Ready) styles.back().contents_status=" [loading]";
                else if (hierarchy.child(id,styles.size()-1)==no_node) {
                    const auto count=filesystem.children(placement.item.members.front()).size();
                    if (count) styles.back().contents_status=" ["+std::to_string(count)+" entries]";
                }
            }
        }
    }
    query_dirty_=false;
}
void Browser::reconcile_selection() {
    auto index=navigation.selected();
    if (const auto key=navigation.context().selection) {
        for (std::size_t i=0;i<layout.objects.size();++i) if (selection_matches(*key,layout.objects[i].item)) { index=i; break; }
    }
    if (index>=layout.objects.size() || !eligible(layout.objects[index].item)) {
        index=no_selection;
        for (std::size_t i=0;i<layout.objects.size();++i) if (eligible(layout.objects[i].item)) { index=i; break; }
    }
    if (index==no_selection) navigation.clear_selection();
    else navigation.select_item(index,layout.objects[index].item);
}
void Browser::select(std::size_t index,bool focus) {
    if (index>=layout.objects.size() || !eligible(layout.objects[index].item)) return;
    const bool changed=navigation.selected()!=index;
    navigation.select_item(index,layout.objects[index].item);
    if (focus) {
        const auto scale=hierarchy.node(current_).transform.scale;
        camera.focus(layout.objects[index].center,animations,13*scale,scale,hierarchy.node(current_).transform.translation.y);
        const auto& item=layout.objects[index].item;
        if (item.group==GroupKind::None && item.category==Category::Directory) request_directory(item.members.front(),false);
    }
    if (changed) refresh_scene();
}
void Browser::move(Direction direction) {
    if (viewport_width<=0 || viewport_height<=0) return;
    const auto matrix=camera.view_projection(viewport_width/viewport_height);
    std::vector<SelectionPoint> points;
    for (std::size_t i=0;i<layout.objects.size();++i) {
        if (!eligible(layout.objects[i].item) || i>=scene.drawn.size() || !scene.drawn[i]) continue;
        const auto p=project(layout.objects[i].center,matrix,viewport_width,viewport_height);
        if (p && p->x>=0 && p->x<viewport_width && p->y>=0 && p->y<viewport_height) points.push_back({i,p->x,p->y});
    }
    const auto next=directional_neighbor(points,navigation.selected()==no_selection ? std::nullopt:std::optional{navigation.selected()},direction);
    if (next) select(*next,false);
}
void Browser::next_match(bool reverse) {
    if (matches_.empty()) { notice="No matching objects in cached scope"; return; }
    const auto page_size=page_capacity(hierarchy.node(current_).items.size(),hierarchy.layout_config);
    auto next=reverse ? matches_.back():matches_.front();
    if (const auto* item=selected_item();item && item->group!=GroupKind::Summary) {
        const auto global=layout.page*page_size+navigation.selected();
        const auto found=std::find(matches_.begin(),matches_.end(),global);
        if (found!=matches_.end()) {
            auto offset=static_cast<std::size_t>(found-matches_.begin());
            offset=reverse ? (offset+matches_.size()-1)%matches_.size():(offset+1)%matches_.size(); next=matches_[offset];
        }
    }
    const auto page=next/page_size;
    if (page!=layout.page) {
        hierarchy.set_page(current_,page); navigation.restore_page(page);
        sync_view(); schedule_previews();
    }
    select(next%page_size,true);
}
void Browser::act(UiAction action) {
    switch (action.kind) {
    case ActionKind::None: return;
    case ActionKind::Focus: select(action.index,true); break;
    case ActionKind::Select: select(action.index,false); break;
    case ActionKind::Left: move(Direction::Left); break;
    case ActionKind::Right: move(Direction::Right); break;
    case ActionKind::Above: move(Direction::Up); break;
    case ActionKind::Below: move(Direction::Down); break;
    case ActionKind::Next:
    case ActionKind::Previous: {
        if (layout.pages<=1) return;
        const bool reverse=action.kind==ActionKind::Previous;
        const auto page=reverse ? (layout.page+layout.pages-1)%layout.pages:(layout.page+1)%layout.pages;
        hierarchy.set_page(current_,page); navigation.restore_page(page);
        sync_view(); schedule_previews();
        break;
    }
    case ActionKind::Open: {
        if (action.index>=layout.objects.size() || !eligible(layout.objects[action.index].item)) return;
        cancel_parent();
        select(action.index,false);
        const auto child=hierarchy.child(current_,action.index);
        save_view();
        const auto& opening=layout.objects[action.index].item;
        if (loader_ && child!=no_node && opening.group==GroupKind::None && opening.category==Category::Directory &&
            hierarchy.node(child).depth>=std::min(3u,hierarchy.config.max_depth)) {
            enter_rebased(child,opening.members.front()); break;
        }
        if (child!=no_node && navigation.open(filesystem,layout.objects[action.index].item)) {
            parents_.push_back(current_); current_=child;
            navigation.restore_page(hierarchy.node(current_).world.page);
            hierarchy.ensure_preview(current_,hierarchy.config.preview_depth);
            sync_view(); focus_overview();
            request_directory(navigation.context().directory,true,true); schedule_previews(); record_view();
        } else {
            const auto& item=layout.objects[action.index].item;
            if (loader_ && item.group==GroupKind::None) {
                const auto& entry=filesystem.entry(item.members.front());
                if (entry.kind==EntryKind::File || entry.kind==EntryKind::Symlink || entry.kind==EntryKind::Directory)
                    validate(entry.full_path,{Purpose::Activate},entry.kind==EntryKind::Directory);
                else notice="This object cannot be opened";
            } else select(action.index,true);
        }
        break;
    }
    case ActionKind::Up:
        save_view();
        if (navigation.up()) {
            apply_deferred_=true; current_=parents_.back(); parents_.pop_back();
            sync_view(); focus_overview(); schedule_previews(); record_view();
        } else if (loader_) validate(std::filesystem::path(filesystem.path(root_id))/"..",{Purpose::Parent,0,0,generation_},true);
        break;
    case ActionKind::PreviousPage:
    case ActionKind::NextPage:
        if ((action.kind==ActionKind::PreviousPage && layout.page==0) ||
            (action.kind==ActionKind::NextPage && layout.page+1>=layout.pages)) return;
        navigation.page(action.kind==ActionKind::PreviousPage ? -1:1,layout.pages);
        hierarchy.set_page(current_,navigation.context().page);
        sync_view(); focus_overview(); schedule_previews(); break;
    case ActionKind::Reset: focus_overview(); break;
    case ActionKind::Search: query_.text=std::move(action.text); query_dirty_=true; refresh_scene(); break;
    case ActionKind::SearchActivate:
        query_.text=std::move(action.text); query_dirty_=true; refresh_scene();
        act({ActionKind::Open,navigation.selected()}); break;
    case ActionKind::Filter: query_.filter=action.filter; query_dirty_=true; refresh_scene(); break;
    case ActionKind::NextMatch: next_match(false); break;
    case ActionKind::PreviousMatch: next_match(true); break;
    case ActionKind::Navigate: navigate(std::move(action.text)); break;
    case ActionKind::Back: go_history(-1); break;
    case ActionKind::Forward: go_history(1); break;
    case ActionKind::Terminal: terminal_here(); break;
    case ActionKind::Cancel:
        pending_parent_.reset();
        std::erase_if(validations_,[](const auto& entry) { return entry.second.purpose!=Purpose::Surroundings; }); location_tag_=0;
        notice.clear(); query_.text.clear(); query_dirty_=true; refresh_scene(); break;
    }
}
}
