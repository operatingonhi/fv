#include "app/browser.hpp"
#include "navigation/path.hpp"
#include <algorithm>
#include <cstdlib>
#include <set>

namespace spatial {
void Browser::save_view() {
    if (history.entries().empty()) return;
    views_[history.current().state]={world_,navigation.snapshot(),current_,parents_,camera.settled_state(),query_};
}
void Browser::record_view() {
    if (location_tag_) { validations_.erase(location_tag_); location_tag_=0; }
    const auto token=next_state_++;
    views_[token]={world_,navigation.snapshot(),current_,parents_,camera.settled_state(),query_};
    history.push({filesystem.path(navigation.context().directory),token});
    std::set<std::size_t> retained;
    for (const auto& entry:history.entries()) retained.insert(entry.state);
    for (auto it=views_.begin();it!=views_.end();) {
        if (!retained.contains(it->first)) it=views_.erase(it); else ++it;
    }
}
void Browser::stash_world() {
    filesystem.clear_loading();
    world_->filesystem=std::move(filesystem); world_->hierarchy=std::move(hierarchy);
    pending_.clear(); deferred_.clear(); apply_deferred_=false; ++generation_;
    if (loader_) loader_->discard_reads_except(generation_);
    levels_.clear(); scratch_levels_.clear();
}
void Browser::restore_view(std::size_t state) {
    const auto found=views_.find(state);
    if (found==views_.end()) { notice="History state is no longer available"; return; }
    const auto view=found->second;
    const bool same_world=view.world==world_;
    if (!same_world) {
        stash_world(); world_=view.world;
        filesystem=std::move(*world_->filesystem); hierarchy=std::move(*world_->hierarchy);
        world_->filesystem.reset(); world_->hierarchy.reset();
    }
    // Restore ancestor pages before resolving child nodes. A history entry can
    // refer to a different page, or an old node cache replaced by arriving data.
    auto contexts=view.navigation.parents; contexts.push_back(view.navigation.current);
    Navigation restored; current_=0; parents_.clear(); bool complete=true;
    for (std::size_t level=0;level+1<contexts.size();++level) {
        hierarchy.set_page(current_,contexts[level].page);
        hierarchy.ensure_preview(current_,hierarchy.config.preview_depth);
        const auto& next=contexts[level+1];
        const auto matches=[&](const Item& item) {
            if (next.directory!=contexts[level].directory)
                return item.group==GroupKind::None && item.category==Category::Directory &&
                    std::find(item.members.begin(),item.members.end(),next.directory)!=item.members.end();
            return next.group && item.group!=GroupKind::None && item.label==next.group->label &&
                !next.group->members.empty() && std::find(item.members.begin(),item.members.end(),next.group->members.front())!=item.members.end();
        };
        auto locate=[&]()->std::optional<std::size_t> {
            const auto& objects=hierarchy.node(current_).world.objects;
            for (std::size_t i=0;i<objects.size();++i) if (matches(objects[i].item)) return i;
            return {};
        };
        auto index=locate();
        if (!index) {
            const auto items=hierarchy.node(current_).items;
            for (std::size_t i=0;i<items.size();++i) if (matches(items[i])) {
                hierarchy.set_page(current_,i/page_capacity(items.size(),hierarchy.layout_config)); index=locate(); break;
            }
        }
        if (!index || hierarchy.child(current_,*index)==no_node) { complete=false; break; }
        const auto item=hierarchy.node(current_).world.objects[*index].item;
        restored.restore_page(hierarchy.node(current_).world.page); restored.select_item(*index,item);
        restored.open(filesystem,item); parents_.push_back(current_); current_=hierarchy.child(current_,*index);
    }
    hierarchy.set_page(current_,view.navigation.current.page);
    auto snapshot=restored.snapshot(); snapshot.current.page=hierarchy.node(current_).world.page;
    if (complete) { snapshot.current.selected=view.navigation.current.selected; snapshot.current.selection=view.navigation.current.selection; }
    navigation.restore(std::move(snapshot)); query_=view.query; apply_deferred_=true;
    sync_view();
    if (complete) camera.restore(view.camera,animations && same_world);
    else { focus_overview(); notice="History group changed; restored the nearest available context"; }
    request_directory(navigation.context().directory,true,true); schedule_previews();
}
void Browser::validate(const std::filesystem::path& path,Validation action,bool directory_only) {
    if (!loader_) return;
    const auto tag=next_validation_++;
    if (action.purpose==Purpose::Location || action.purpose==Purpose::History) {
        if (location_tag_) validations_.erase(location_tag_);
        location_tag_=tag;
    }
    validations_[tag]=action; loader_->validate(tag,path,directory_only);
    notice="Checking "+path.string();
}
void Browser::navigate(std::string text) {
    if (!loader_) { notice="Path entry requires a real filesystem source"; return; }
    if (location_tag_) { validations_.erase(location_tag_); location_tag_=0; }
    const auto* home=std::getenv("HOME");
    const auto parsed=parse_location(std::move(text),filesystem.path(navigation.context().directory),home ? home:"");
    if (!parsed.error.empty()) { notice=parsed.error; return; }
    validate(parsed.path,{Purpose::Location},true);
}
void Browser::go_history(int delta) {
    const auto target=history.target(delta);
    if (!target) return;
    save_view();
    if (loader_) validate(history.at(*target).directory,{Purpose::History,*target,history.at(*target).state},true);
    else { history.move_to(*target); restore_view(history.current().state); }
}
bool Browser::enter_existing(NodeId target) {
    save_view();
    std::vector<NodeId> chain;
    for (auto id=target;id!=0;id=hierarchy.node(id).parent) chain.push_back(id);
    std::reverse(chain.begin(),chain.end());
    Navigation restored; std::vector<NodeId> parents; NodeId parent=0;
    for (auto id:chain) {
        const auto& node=hierarchy.node(parent);
        const auto found=std::find(node.children.begin(),node.children.end(),id);
        if (found==node.children.end()) return false;
        const auto index=static_cast<std::size_t>(found-node.children.begin());
        restored.restore_page(node.world.page); restored.select_item(index,node.world.objects[index].item);
        if (!restored.open(filesystem,node.world.objects[index].item)) return false;
        parents.push_back(parent); parent=id;
    }
    navigation=std::move(restored); parents_=std::move(parents); current_=target;
    navigation.restore_page(hierarchy.node(current_).world.page);
    hierarchy.ensure_preview(current_,hierarchy.config.preview_depth);
    apply_deferred_=true; query_.text.clear();
    sync_view(); focus_overview(); request_directory(navigation.context().directory,true,true);
    schedule_previews(); record_view(); return true;
}
void Browser::open_location(const std::filesystem::path& path) {
    for (auto node:hierarchy.visible_nodes()) {
        const auto directory=hierarchy.node(node).directory;
        if (directory && std::filesystem::path(filesystem.path(*directory)).lexically_normal()==path) {
            if (node==current_ && !navigation.context().group) {
                query_.text.clear(); query_dirty_=true; refresh_scene(); focus_overview(); notice="Already at this location";
                return;
            }
            if (enter_existing(node)) return;
        }
    }
    save_view(); stash_world(); world_=std::make_shared<World>();
    filesystem=Filesystem(path.string()); navigation=Navigation{}; hierarchy=Hierarchy{};
    current_=0; parents_.clear(); query_.text.clear();
    hierarchy.initialize({DirectoryItems{0,{}}},{});
    camera.focus(hierarchy.focus(0),false,24);
    sync_view(); request_directory(0,true); record_view();
}
void Browser::terminal_here() {
    auto directory=navigation.context().directory;
    if (const auto* item=selected_item();item && item->group==GroupKind::None) {
        const auto& entry=filesystem.entry(item->members.front());
        if (entry.kind==EntryKind::Directory) directory=entry.id;
    }
    validate(filesystem.path(directory),{Purpose::Terminal},true);
}
void Browser::process_validation(PathValidation result) {
    const auto pending=validations_.find(result.tag);
    if (pending==validations_.end()) return;
    const auto action=pending->second; validations_.erase(pending);
    if (result.tag==location_tag_) location_tag_=0;
    if (!result.error.empty()) {
        if (action.purpose==Purpose::History && action.history_index<history.entries().size() &&
            history.at(action.history_index).state==action.history_state) history.invalidate(action.history_index);
        notice=result.error; return;
    }
    switch (action.purpose) {
    case Purpose::Location: notice.clear(); open_location(result.resolved); break;
    case Purpose::History:
        if (action.history_index>=history.entries().size() || history.at(action.history_index).state!=action.history_state) {
            notice="History changed; navigation cancelled"; return;
        }
        save_view(); history.move_to(action.history_index); notice.clear(); restore_view(action.history_state); break;
    case Purpose::Activate:
        if (result.metadata.kind==EntryKind::Directory) { open_location(result.resolved); notice.clear(); }
        else {
            const auto error=launcher_.launch(file_command(result.resolved));
            notice=error.empty() ? "Default-application launch requested":error;
        }
        break;
    case Purpose::Terminal: {
        const auto setting=terminal_environment();
        auto plan=terminal_command(result.resolved,setting);
        auto error=launcher_.launch(plan);
        if (!error.empty() && setting.empty()) {
            plan.arguments={"xterm"};
            const auto fallback=launcher_.launch(plan);
            error=fallback.empty() ? "":error+"; "+fallback;
        }
        notice=error.empty() ? "Terminal launch requested in "+result.resolved.string():error;
        break;
    }
    }
}
}
