#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace spatial {
using EntryId=std::size_t;
inline constexpr EntryId root_id=0;
enum class EntryKind { Directory, File, Symlink, Other };
struct Entry {
    EntryId id=0, parent=0;
    std::string name;
    EntryKind kind=EntryKind::Other;
    std::int64_t modified=0;
    bool executable=false;
    std::uintmax_t bytes=0;
    bool size_known=false, modified_known=true, hidden=false, broken_link=false;
    std::filesystem::perms permissions=std::filesystem::perms::unknown;
    std::string full_path{}, link_target{}, error{};
};
struct DirectoryListing {
    Entry directory;
    std::vector<Entry> entries;
    bool complete=true;
    std::string error;
};
enum class LoadState { Unloaded, Preview, Ready, Error };
struct DirectoryState {
    LoadState state=LoadState::Unloaded;
    bool loading=false;
    std::string message;
};
// Main-thread metadata cache. This class performs no filesystem I/O.
class Filesystem {
public:
    explicit Filesystem(std::string root="/mock");
    EntryId add(EntryId parent,std::string name,EntryKind kind,std::int64_t modified,bool executable=false);
    const Entry& entry(EntryId id) const { return entries_.at(id); }
    const std::vector<EntryId>& children(EntryId parent) const { return children_.at(parent); }
    std::string path(EntryId id) const { return entry(id).full_path; }
    std::size_t size() const { return entries_.size(); }
    const DirectoryState& state(EntryId id) const { return states_.at(id); }
    void mark_loading(EntryId id) { states_.at(id).loading=true; }
    void clear_loading() { for (auto& state:states_) state.loading=false; }
    void apply(EntryId directory,DirectoryListing listing);
private:
    std::vector<Entry> entries_;
    std::vector<std::vector<EntryId>> children_;
    std::vector<std::map<std::string,EntryId>> names_;
    std::vector<DirectoryState> states_;
};
}
