#pragma once
#include "filesystem/mock_filesystem.hpp"
#include <span>
#include <string>
#include <vector>

namespace spatial {
enum class Category { Directory, Text, Source, Image, Media, Archive, Executable, Binary, Generated, Mixed };
enum class FileShape { Diorama, Document, ImagePanel, MediaPanels, ArchiveStack, LowTile, Prism };
FileShape file_shape(Category category);
enum class GroupKind { None, Series, Related, Summary };
struct AnalysisConfig {
    double recency_half_life=7*86400.0;
    float generated_weight=0.22f, binary_weight=0.38f, executable_weight=0.55f;
    std::int64_t related_window=600;
    std::size_t minimum_group=3;
};
struct Item {
    std::string label;
    Category category=Category::Text;
    GroupKind group=GroupKind::None;
    float salience=0;
    std::vector<EntryId> members;
};
Category classify(const Entry& entry);
float salience(const Entry& entry,std::int64_t now,const AnalysisConfig& config={});
// Group members are IDs; no positions, rendering state, or filesystem traversal.
std::vector<Item> analyze(const MockFilesystem& fs,std::span<const EntryId> entries,
    std::int64_t now,const AnalysisConfig& config={},bool grouping=true);
const char* category_name(Category category);
}
