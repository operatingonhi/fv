#pragma once
#include "filesystem/model.hpp"
#include <span>
#include <string>
#include <vector>

namespace spatial {
enum class Category { Directory, Text, Source, Image, Media, Archive, Executable, Binary, Generated, Symlink, Special, Mixed };
enum class FileShape { Diorama, Document, CodeSlab, ExecutableFrame, ImagePanel, MediaPanels, ArchiveStack, LowTile, Linked, Prism };
FileShape file_shape(Category category);
enum class ArtifactKind { None, Object, Library, Dependency, Cache, Temporary, GeneratedSource };
ArtifactKind artifact_kind(const Entry& entry);
enum class GroupKind { None, Series, Related, Artifacts, Summary };
struct AnalysisConfig {
    double recency_half_life=7*86400.0;
    float recency_weight=0.24f;
    float generated_weight=0.22f, binary_weight=0.55f, executable_weight=0.85f;
    float hidden_weight=0.65f, directory_bonus=0.06f, group_bonus=0.04f;
    std::int64_t related_window=600;
    double related_size_ratio=8, series_size_ratio=32;
    std::size_t minimum_group=3, artifact_summary_minimum=8;
};
struct SalienceContext {
    std::size_t known_children=0;
    bool children_complete=false;
    float generated_fraction=0;
};
struct SalienceBreakdown {
    float semantic_base=0, recency_bonus=0, directory_bonus=0;
    float artifact_multiplier=1, hidden_multiplier=1, total=0;
};
struct Item {
    std::string label;
    Category category=Category::Text;
    GroupKind group=GroupKind::None;
    float salience=0;
    std::vector<EntryId> members;
    std::string explanation{};
};
Category classify(const Entry& entry);
SalienceBreakdown salience_breakdown(const Entry& entry,std::int64_t now,const AnalysisConfig& config={},SalienceContext context={});
float salience(const Entry& entry,std::int64_t now,const AnalysisConfig& config={},SalienceContext context={});
float group_salience(std::span<const float> scores,const AnalysisConfig& config={});
// Group members are IDs; no positions, rendering state, or filesystem traversal.
std::vector<Item> analyze(const Filesystem& fs,std::span<const EntryId> entries,
    std::int64_t now,const AnalysisConfig& config={},bool grouping=true);
const char* category_name(Category category);
}
