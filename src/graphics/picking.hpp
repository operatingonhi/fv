#pragma once
#include "graphics/camera.hpp"
#include "graphics/scene.hpp"
#include <optional>
#include <span>

namespace spatial {
struct ScreenPoint { float x,y,depth; };
std::optional<ScreenPoint> project(Vec3 point,const Mat4& matrix,float width,float height);
// Coordinates are relative to the scene viewport, in the same units as width/height.
std::optional<std::size_t> pick(std::span<const SceneBox> boxes,const Camera& camera,
    float x,float y,float width,float height);
}
