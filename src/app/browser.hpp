#pragma once
#include "ui/overlay.hpp"

namespace spatial {
// Application orchestration: connects semantic navigation, layout, presentation,
// and camera requests. No window or OpenGL dependency, so it is unit-testable.
class Browser {
public:
    Browser();
    MockFilesystem filesystem=make_mock_filesystem();
    Navigation navigation;
    Layout layout;
    Presentation scene;
    Camera camera;
    bool animations=true;
    std::size_t scene_revision=0;
    void act(UiAction action);
private:
    void rebuild();
    void focus_overview();
    Vec3 origin_{};
    std::vector<Vec3> parent_origins_;
};
}
