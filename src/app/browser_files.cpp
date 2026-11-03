#include "app/browser.hpp"
#include "navigation/path.hpp"
#include <cstdlib>

namespace spatial {
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
        if (!cut_file_) { notice="Cut an individual file or directory first"; return; }
        auto operation=*cut_file_;
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
    FileOperation operation{action.kind==ActionKind::DeleteFile ? FileOperationKind::Delete:FileOperationKind::Move,
        entry.full_path,{},entry.identity};
    if (action.kind==ActionKind::Cut) {
        file_prompt_.reset(); cut_file_=std::move(operation);
        notice="Cut: "+entry.full_path+". Navigate to a destination and paste to move it.";
    } else {
        pending_parent_.reset(); location_tag_=0;
        std::erase_if(validations_,[](const auto& pending) { return pending.second.purpose!=Purpose::Surroundings; });
        notice.clear();
        operation.destination_directory=filesystem.path(navigation.context().directory);
        file_prompt_=std::move(operation);
    }
}
void Browser::refresh_file_cache() {
    // Group memberships can change after removal. Return to their real directory
    // before refreshing so regrouping cannot detach the active context.
    bool left_group=false;
    while (navigation.context().group && navigation.can_go_up()) {
        navigation.up(); current_=parents_.back(); parents_.pop_back(); left_group=true;
    }
    if (left_group) focus_overview();
    ++generation_; loader_->discard_reads_except(generation_);
    pending_.clear(); deferred_.clear(); apply_deferred_=false;
    validations_.clear(); location_tag_=0; pending_parent_.reset();
    filesystem.invalidate(); surroundings_.reset();
    world_->file_epoch=file_epoch_;
    request_directory(navigation.context().directory,true,true);
    schedule_previews(); request_surroundings();
    sync_view(); save_view();
}
void Browser::process_file_result(FileOperationResult result) {
    file_busy_=false;
    if (result.success && cut_file_ && result.operation.source==cut_file_->source && result.operation.expected==cut_file_->expected)
        cut_file_.reset();
    // Failures may reveal external changes too (a missing/replaced source or a
    // newly occupied destination). Refresh cached metadata before another try.
    ++file_epoch_; refresh_file_cache();
    if (result.success) notice=result.operation.kind==FileOperationKind::Delete ?
        "Deleted: "+result.operation.source.string():"Moved to: "+result.destination.string();
    else notice=result.error;
}
}
