#pragma once
#include "layout/layout.hpp"
#include "graphics/scene.hpp"

namespace spatial {
Vec3 semantic_color(Category category);
struct SceneLabel { Vec3 anchor,color; std::string text; bool visible,selected; };
struct Presentation {
    Vec3 grid_origin{};
    std::vector<SceneBox> boxes, targets;
    std::vector<SceneLabel> labels;
};
Presentation present(const Layout& layout,std::size_t selected);
}
