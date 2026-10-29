#pragma once
#include "graphics/scene.hpp"
#include <imgui.h>
#include <span>
#include <string_view>
#include <vector>

namespace spatial {
struct InscriptionVertex { Vec3 position, color; ImVec2 uv; };
// Use the embedded scalable font at high resolution for scene lettering.
ImFont* add_inscription_font(ImFontAtlas& atlas);
// Width is in the fixed font's pixel units. Truncation preserves UTF-8 boundaries.
std::string fit_inscription(ImFont& font,std::string_view value,float width);
// CPU-only construction, independent of camera and OpenGL. Atlas must be built.
std::vector<InscriptionVertex> inscription_mesh(ImFontAtlas& atlas,std::span<const SceneLabel> labels);
}
