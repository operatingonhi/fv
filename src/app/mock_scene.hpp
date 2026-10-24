#pragma once
#include "graphics/scene.hpp"
#include <array>
#include <string_view>

namespace spatial {
// Fixed display fixtures only: no filesystem model, scan, classification or layout engine.
struct MockDirectory { std::string_view path; Vec3 center; };
inline constexpr std::array<MockDirectory,2> directories{{
    {"/mock/projects",{-4,0,0}}, {"/mock/documents",{4,0,-3}}
}};
inline constexpr std::array<SceneBox,8> mock_boxes{{
    {{-4,0.15f,0},{4.6f,0.3f,3.8f},{0.25f,0.65f,1}},
    {{-4,1.0f,-1.5f},{4.6f,1.7f,0.15f},{0.25f,0.65f,1}},
    {{-5,0.55f,0.4f},{0.9f,0.8f,1.1f},{0.72f,0.83f,0.82f}},
    {{-3,0.4f,0.4f},{0.9f,0.5f,1.1f},{0.72f,0.83f,0.82f}},
    {{4,0.15f,-3},{4.6f,0.3f,3.8f},{0.25f,0.65f,1}},
    {{4,1.0f,-4.5f},{4.6f,1.7f,0.15f},{0.25f,0.65f,1}},
    {{3,0.6f,-2.5f},{0.9f,0.9f,1.1f},{0.72f,0.83f,0.82f}},
    {{5,0.4f,-2.5f},{0.9f,0.5f,1.1f},{0.72f,0.83f,0.82f}}
}};
}
