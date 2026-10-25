#pragma once
#include "analysis/analysis.hpp"
#include <optional>

namespace spatial {
enum class EntryFilter { All, Files, Directories };
struct FilterSpec {
    EntryFilter kind=EntryFilter::All;
    std::optional<Category> category;
    bool include_hidden=true, include_artifacts=true;
    bool operator==(const FilterSpec&) const=default;
};
struct QuerySpec {
    std::string text;
    FilterSpec filter;
    bool active() const;
};
struct QueryResult {
    std::vector<unsigned char> eligible;
    bool incomplete=false;
};
bool name_matches(std::string_view name,std::string_view query);
bool filter_matches(const Entry& entry,const FilterSpec& filter);
// Cache-only current entries plus at most two loaded descendant levels. No I/O.
QueryResult match_cached(const Filesystem& filesystem,std::span<const EntryId> roots,const QuerySpec& query,unsigned depth=2);
bool item_matches(const Item& item,const QueryResult& result);
}
