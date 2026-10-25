#include "analysis/query.hpp"
#include <algorithm>
#include <cctype>

namespace spatial {
namespace {
unsigned char folded(char c) { return static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(c))); }
}
bool QuerySpec::active() const { return !text.empty() || filter!=FilterSpec{}; }
bool name_matches(std::string_view name,std::string_view query) {
    return std::search(name.begin(),name.end(),query.begin(),query.end(),[](char a,char b) { return folded(a)==folded(b); })!=name.end() || query.empty();
}
bool filter_matches(const Entry& entry,const FilterSpec& filter) {
    if (!filter.include_hidden && (entry.hidden || entry.name.starts_with('.'))) return false;
    if (!filter.include_artifacts && artifact_kind(entry)!=ArtifactKind::None) return false;
    if (filter.kind==EntryFilter::Directories && entry.kind!=EntryKind::Directory) return false;
    if (filter.kind==EntryFilter::Files && entry.kind==EntryKind::Directory) return false;
    return !filter.category || classify(entry)==*filter.category;
}
QueryResult match_cached(const Filesystem& fs,std::span<const EntryId> roots,const QuerySpec& query,unsigned depth) {
    QueryResult result; result.eligible.resize(fs.size(),0);
    const auto visit=[&](auto&& self,EntryId id,unsigned remaining)->bool {
        const auto& entry=fs.entry(id);
        if ((!query.filter.include_hidden && (entry.hidden || entry.name.starts_with('.'))) ||
            (!query.filter.include_artifacts && artifact_kind(entry)!=ArtifactKind::None)) return false;
        bool matches=filter_matches(entry,query.filter) && name_matches(entry.name,query.text);
        if (entry.kind==EntryKind::Directory) {
            if (fs.state(id).state!=LoadState::Ready) result.incomplete=true;
            if (remaining>0) for (auto child:fs.children(id)) matches=self(self,child,remaining-1) || matches;
        }
        result.eligible[id]=matches;
        return matches;
    };
    for (auto id:roots) visit(visit,id,depth);
    return result;
}
bool item_matches(const Item& item,const QueryResult& result) {
    return std::any_of(item.members.begin(),item.members.end(),[&](auto id) { return id<result.eligible.size() && result.eligible[id]; });
}
}
