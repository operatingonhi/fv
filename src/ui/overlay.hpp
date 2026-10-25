#pragma once
#include "graphics/camera.hpp"
#include "navigation/navigation.hpp"
#include "ui/presentation.hpp"

namespace spatial {
enum class ActionKind { None, Focus, Open, Up, PreviousPage, NextPage, Reset };
struct UiAction { ActionKind kind=ActionKind::None; std::size_t index=0; };
UiAction overlay(const MockFilesystem& fs,const Navigation& navigation,const Layout& layout,
    const Camera& camera,bool& animations,float panel_width);
void scene_labels(const Presentation& scene,const Camera& camera,float left,float width,float height);
}
