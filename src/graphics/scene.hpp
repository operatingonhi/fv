#pragma once
#include "graphics/math.hpp"
#include <string>
namespace spatial {
// An inscription lies on an object face. Anchor is its center;
// extent/height bound its text area. It never rotates to face the camera.
struct SceneLabel { Vec3 anchor,color; std::string text; bool visible,selected; float extent=1; bool required=false; float height=0.25f; Vec3 up{0,1,0}; };
struct SceneBox { Vec3 center, size, color; };
}
