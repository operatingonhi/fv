#pragma once
#include "layout/hierarchy.hpp"
#include "graphics/camera.hpp"
#include "graphics/scene.hpp"
#include "graphics/grid.hpp"
#include <unordered_map>
#include <string_view>

namespace spatial {
inline constexpr Vec3 selection_color{1,0.95f,0.5f};
struct PresentationConfig { unsigned nested_depth=3; };
std::string display_name(std::string_view name);
Vec3 semantic_color(Category category);
SceneLabel backdrop_inscription(LocalTransform transform,std::string name,Vec3 color,bool required=false);
struct ObjectAppearance { float brightness=1; bool selectable=true, force_detail=false; std::string contents_status{}; };
using AppearanceMap=std::unordered_map<NodeId,std::vector<ObjectAppearance>>;
struct Presentation {
    std::string context_name;
    GridFrame grid{};
    std::vector<SceneBox> boxes, targets;
    std::vector<SceneLabel> labels;
    std::vector<unsigned char> interactive,drawn;
};
// Geometry LOD depends on apparent world scale, not which directory was activated.
void detail_levels(const Hierarchy& hierarchy,const Camera& camera,std::vector<unsigned char>& result,std::span<const unsigned char> previous={});
Presentation present(const Hierarchy& hierarchy,NodeId current,std::size_t selected,
    std::span<const unsigned char> levels,const AppearanceMap& appearance={},const PresentationConfig& config={},NodeId excluded=no_node);
}
