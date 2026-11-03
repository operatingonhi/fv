#include "app/browser.hpp"
#include "navigation/path.hpp"
#include <cstdlib>

namespace spatial {
namespace {
void erase_source(const FileOperationResult& result,Filesystem& filesystem,Hierarchy& hierarchy) {
    std::vector<EntryId> removed;
    const auto name=result.operation.source.filename().string();
    for (EntryId id=1;id<filesystem.size();++id) {
        const auto& entry=filesystem.entry(id);
        const auto& parent=filesystem.entry(entry.parent).identity;
        // Another hard link with this filename in a different directory must
        // remain visible. Match the parent inode too, including aliased paths.
        if (entry.name==name && entry.identity.known && result.source_parent.known && parent.known &&
            entry.identity.device==result.operation.expected.device && entry.identity.inode==result.operation.expected.inode &&
            parent.device==result.source_parent.device && parent.inode==result.source_parent.inode) removed.push_back(id);
    }
    for (auto id:removed) filesystem.erase(id);
    hierarchy.erase_entries(removed);
}
}
void Browser::begin_file_operation(FileOperation operation) {
    pending_parent_.reset(); location_tag_=0;
    std::erase_if(validations_,[](const auto& entry) { return entry.second.purpose!=Purpose::Surroundings; });
    loader_->operate(std::move(operation)); file_busy_=true;
    notice="File operation in progress...";
}
bool Browser::can_change_selection() const {
    if (!loader_ || file_busy_) return false;
    const auto* item=selected_item();
    if (!item || !eligible(*item) || item->group!=GroupKind::None || item->members.size()!=1) return false;
    const auto& entry=filesystem.entry(item->members.front());
    return filesystem.state(navigation.context().directory).state==LoadState::Ready &&
        !filesystem.state(navigation.context().directory).loading &&
        entry.id!=navigation.context().directory && entry.identity.known &&
        (entry.kind==EntryKind::File || entry.kind==EntryKind::Directory || entry.kind==EntryKind::Symlink);
}
void Browser::file_action(UiAction action) {
    if (action.kind==ActionKind::CancelFileOperation) { file_prompt_.reset(); return; }
    if (!loader_) { notice="File operations require a real filesystem"; return; }
    if (file_busy_) { notice="A file operation is still running"; return; }
    if (action.kind==ActionKind::Paste) {
        if (!file_clipboard_) { notice="Copy or cut an individual file or directory first"; return; }
        auto operation=*file_clipboard_;
        operation.destination_directory=filesystem.path(navigation.context().directory);
        begin_file_operation(std::move(operation)); return;
    }
    if (action.kind==ActionKind::ConfirmFileOperation) {
        if (!file_prompt_) { notice="No file operation is awaiting confirmation"; return; }
        auto operation=*file_prompt_;
        if (operation.kind==FileOperationKind::Move) {
            const auto* home=std::getenv("HOME");
            const auto destination=parse_location(std::move(action.text),filesystem.path(navigation.context().directory),home ? home:"");
            if (!destination.error.empty()) { notice=destination.error; return; }
            operation.destination_directory=destination.path;
        }
        file_prompt_.reset(); begin_file_operation(std::move(operation)); return;
    }
    if (!can_change_selection()) { notice="Select one file, directory or symlink; expand groups to select a member"; return; }
    const auto& entry=filesystem.entry(selected_item()->members.front());
    const auto kind=action.kind==ActionKind::DeleteFile ? FileOperationKind::Delete:
        action.kind==ActionKind::Copy ? FileOperationKind::Copy:FileOperationKind::Move;
    FileOperation operation{kind,
        entry.full_path,{},entry.identity};
    if (action.kind==ActionKind::Cut || action.kind==ActionKind::Copy) {
        file_prompt_.reset(); file_clipboard_=std::move(operation);
        notice=(action.kind==ActionKind::Copy ? "Copied: ":"Cut: ")+entry.full_path+
            ". Navigate to a destination and paste to "+(action.kind==ActionKind::Copy ? "copy it.":"move it.");
    } else {
        pending_parent_.reset(); location_tag_=0;
        std::erase_if(validations_,[](const auto& pending) { return pending.second.purpose!=Purpose::Surroundings; });
        notice.clear();
        operation.destination_directory=filesystem.path(navigation.context().directory);
        file_prompt_=std::move(operation);
    }
}
void Browser::refresh_file_cache(const FileOperationResult* operation) {
    // Group memberships can change after removal. Return to their real directory
    // before refreshing so regrouping cannot detach the active context.
    bool left_group=false;
    while (navigation.context().group && navigation.can_go_up()) {
        navigation.up(); current_=parents_.back(); parents_.pop_back(); left_group=true;
    }
    if (left_group) focus_overview();
    if (operation && operation->success && operation->operation.kind!=FileOperationKind::Copy) {
        // Ancestor listing updates remain deferred to avoid regrouping an active
        // child. Remove the confirmed source without changing surviving groups
        // or the active parent/child chain, including in stashed history worlds.
        erase_source(*operation,filesystem,hierarchy);
        std::set<World*> visited;
        for (auto& [state,view]:views_) {
            (void)state;
            if (view.world->filesystem && view.world->hierarchy && visited.insert(view.world.get()).second)
                erase_source(*operation,*view.world->filesystem,*view.world->hierarchy);
        }
    }
    ++generation_; loader_->discard_reads_except(generation_);
    pending_.clear(); deferred_.clear(); apply_deferred_=false;
    validations_.clear(); location_tag_=0; pending_parent_.reset();
    filesystem.invalidate(); surroundings_.reset();
    world_->file_epoch=file_epoch_;
    sync_view();
    request_directory(navigation.context().directory,true,true);
    schedule_previews(); request_surroundings();
    save_view();
}
void Browser::process_file_result(FileOperationResult result) {
    file_busy_=false;
    if (result.success && result.operation.kind!=FileOperationKind::Copy && file_clipboard_ &&
        result.operation.source==file_clipboard_->source && result.operation.expected==file_clipboard_->expected)
        file_clipboard_.reset();
    // Failures may reveal external changes too (a missing/replaced source or a
    // newly occupied destination). Refresh cached metadata before another try.
    ++file_epoch_; refresh_file_cache(&result);
    if (result.success) notice=result.operation.kind==FileOperationKind::Delete ? "Deleted: "+result.operation.source.string():
        (result.operation.kind==FileOperationKind::Copy ? "Copied to: ":"Moved to: ")+result.destination.string();
    else notice=result.error;
}
}
