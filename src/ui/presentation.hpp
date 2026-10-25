#pragma once
#include "layout/hierarchy.hpp"
#include "graphics/camera.hpp"
#include "graphics/scene.hpp"
#include <unordered_map>
#include <string_view>

namespace spatial {
inline constexpr Vec3 selection_color{1,0.95f,0.5f};
struct PresentationConfig { unsigned nested_depth=3; };
std::string display_name(std::string_view name);
Vec3 semantic_color(Category category);
struct SceneLabel { Vec3 anchor,color; std::string text; bool visible,selected; float extent=1; bool required=false; };
struct ObjectAppearance { float brightness=1; bool selectable=true, force_detail=false; std::string contents_status{}; };
using AppearanceMap=std::unordered_map<NodeId,std::vector<ObjectAppearance>>;
struct Presentation {
    std::string context_name;
    Vec3 grid_origin{};
    std::vector<SceneBox> boxes, targets;
    std::vector<SceneLabel> labels;
    std::vector<unsigned char> interactive,drawn;
};
// Geometry LOD depends on apparent world scale, not which directory was activated.
void detail_levels(const Hierarchy& hierarchy,const Camera& camera,std::vector<unsigned char>& result,std::span<const unsigned char> previous={});
Presentation present(const Hierarchy& hierarchy,NodeId current,std::size_t selected,
    std::span<const unsigned char> levels,const AppearanceMap& appearance={},const PresentationConfig& config={},NodeId excluded=no_node);
}
