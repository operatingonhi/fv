#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace spatial {
using EntryId = std::size_t;
inline constexpr EntryId root_id=0;
inline constexpr std::int64_t mock_now=1'800'000'000;
enum class EntryKind { Directory, File };
struct Entry {
    EntryId id, parent;
    std::string name;
    EntryKind kind;
    std::int64_t modified;
    bool executable=false;
};
// Entirely in memory. Timestamps are fixed Unix-like seconds, not the host clock.
class MockFilesystem {
public:
    MockFilesystem();
    EntryId add(EntryId parent,std::string name,EntryKind kind,std::int64_t modified,bool executable=false);
    const Entry& entry(EntryId id) const { return entries_.at(id); }
    std::vector<EntryId> children(EntryId parent) const;
    std::string path(EntryId id) const;
    std::size_t size() const { return entries_.size(); }
private:
    std::vector<Entry> entries_;
};
MockFilesystem make_mock_filesystem();
}
