#include "filesystem/model.hpp"
#include <stdexcept>
#include <algorithm>
#include <unordered_set>

namespace spatial {
EntryMap Filesystem::copy_subtree(EntryId source,Filesystem& destination,EntryId target) const {
    EntryMap mapping{{source,target}};
    std::vector<EntryId> queue{source};
    for (std::size_t i=0;i<queue.size();++i) {
        const auto id=queue[i], copied_id=mapping.at(id);
        const auto identity=destination.entry(copied_id);
        auto copy=entry(id);
        copy.id=copied_id; copy.parent=identity.parent; copy.full_path=identity.full_path;
        if (copied_id!=root_id || (identity.name!="." && identity.name!=".." && !identity.name.empty())) copy.name=identity.name;
        copy.hidden=copy.name.starts_with('.');
        destination.entries_[copied_id]=std::move(copy);
        destination.states_[copied_id]=state(id); destination.states_[copied_id].loading=false;
        std::vector<EntryId> copied_children;
        for (auto child:children(id)) {
            const auto& item=entry(child);
            const auto found=destination.names_[copied_id].find(item.name);
            const auto copied_child=found==destination.names_[copied_id].end() ?
                destination.add(copied_id,item.name,item.kind,item.modified,item.executable):found->second;
            mapping.emplace(child,copied_child); queue.push_back(child);
            copied_children.push_back(copied_child);
        }
        if (state(id).state==LoadState::Ready) {
            const std::unordered_set<EntryId> retained(copied_children.begin(),copied_children.end());
            std::erase_if(destination.names_[copied_id],[&](const auto& named) {
                return !retained.contains(named.second);
            });
            destination.children_[copied_id]=std::move(copied_children);
        }
    }
    return mapping;
}
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
    const bool remove_missing=states_.at(directory).remove_missing;
    const auto directory_error=listing.directory.error;
    if (listing.directory.kind==EntryKind::Directory) {
        listing.directory.id=directory; listing.directory.parent=entries_.at(directory).parent;
        entries_[directory]=std::move(listing.directory);
    }
    DirectoryState state;
    state.state=listing.error.empty() ? (listing.complete ? LoadState::Ready:LoadState::Preview):LoadState::Error;
    state.message=std::move(listing.error);
    state.remove_missing=remove_missing && (!listing.complete || !state.message.empty());
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
            entries_[id].kind=EntryKind::Other; entries_[id].modified_known=false;
            if (!remove_missing) ++metadata_errors;
        }
        if (remove_missing) {
            auto& children=children_[directory];
            std::erase_if(children,[&](EntryId id) {
                if (std::binary_search(observed.begin(),observed.end(),id)) return false;
                names_[directory].erase(entries_[id].name);
                return true;
            });
        }
    }
    if (metadata_errors) {
        if (!states_[directory].message.empty()) states_[directory].message+="; ";
        states_[directory].message+=std::to_string(metadata_errors)+" entries have metadata/target errors";
    }
}
void Filesystem::invalidate() {
    for (auto& state:states_) {
        state.state=LoadState::Unloaded; state.loading=false;
        state.message.clear(); state.remove_missing=true;
    }
}
void Filesystem::erase(EntryId id) {
    if (id==root_id) return;
    auto& removed=entries_.at(id);
    std::erase(children_.at(removed.parent),id);
    names_.at(removed.parent).erase(removed.name);
    removed.kind=EntryKind::Other; removed.error="Entry removed by file operation";
    removed.modified_known=false;
}
}
