#include "filesystem/model.hpp"
#include <stdexcept>
#include <algorithm>

namespace spatial {
Filesystem::Filesystem(std::string root) {
    Entry entry;
    entry.name=std::filesystem::path(root).filename().string();
    if (entry.name.empty()) entry.name=root;
    entry.kind=EntryKind::Directory; entry.full_path=std::move(root);
    entry.modified_known=false;
    entries_.push_back(std::move(entry));
    children_.resize(1); names_.resize(1); states_.resize(1);
}
EntryId Filesystem::add(EntryId parent,std::string name,EntryKind kind,std::int64_t modified,bool executable) {
    if (entry(parent).kind!=EntryKind::Directory || name.empty() || name.find('/')!=std::string::npos || name=="." || name=="..")
        throw std::invalid_argument("Invalid filesystem entry");
    if (names_.at(parent).contains(name)) throw std::invalid_argument("Duplicate filesystem entry");
    Entry item;
    item.id=entries_.size(); item.parent=parent; item.name=std::move(name);
    item.kind=kind; item.modified=modified; item.executable=executable;
    item.hidden=item.name.starts_with('.');
    item.full_path=(std::filesystem::path(entry(parent).full_path)/item.name).string();
    const auto id=item.id;
    names_[parent].emplace(item.name,id);
    entries_.push_back(std::move(item)); children_.emplace_back(); names_.emplace_back(); states_.emplace_back();
    children_[parent].push_back(id);
    return id;
}
void Filesystem::apply(EntryId directory,DirectoryListing listing) {
    const auto directory_error=listing.directory.error;
    if (listing.directory.kind==EntryKind::Directory) {
        listing.directory.id=directory; listing.directory.parent=entries_.at(directory).parent;
        entries_[directory]=std::move(listing.directory);
    }
    DirectoryState state;
    state.state=listing.error.empty() ? (listing.complete ? LoadState::Ready:LoadState::Preview):LoadState::Error;
    state.message=std::move(listing.error);
    if (!directory_error.empty()) {
        if (!state.message.empty()) state.message+="; ";
        state.message+=directory_error;
    }
    states_.at(directory)=std::move(state);
    std::size_t metadata_errors=0;
    std::vector<EntryId> observed;
    for (auto& item:listing.entries) {
        EntryId id;
        const auto found=names_[directory].find(item.name);
        if (found==names_[directory].end()) id=add(directory,item.name,item.kind,item.modified,item.executable);
        else id=found->second;
        observed.push_back(id);
        item.id=id; item.parent=directory;
        if (!item.error.empty()) ++metadata_errors;
        entries_[id]=std::move(item);
    }
    if (listing.complete && states_[directory].state!=LoadState::Error) {
        std::sort(observed.begin(),observed.end());
        for (auto id:children_[directory]) if (!std::binary_search(observed.begin(),observed.end(),id)) {
            entries_[id].error="Entry disappeared since preview";
            entries_[id].kind=EntryKind::Other; entries_[id].modified_known=false; ++metadata_errors;
        }
    }
    if (metadata_errors) {
        if (!states_[directory].message.empty()) states_[directory].message+="; ";
        states_[directory].message+=std::to_string(metadata_errors)+" entries have metadata/target errors";
    }
}
}
