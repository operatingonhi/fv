#pragma once
#include "ui/overlay.hpp"

namespace spatial {
// Coordinates live in one persistent hierarchy. Navigation only changes the
// active node and asks the existing camera to move to its world-space transform.
class Browser {
public:
    Browser();
    MockFilesystem filesystem=make_mock_filesystem();
    Navigation navigation;
    Hierarchy hierarchy;
    Layout layout;
    Presentation scene;
    Camera camera;
    bool animations=true;
    std::size_t scene_revision=0;
    NodeId current_node() const { return current_; }
    void act(UiAction action);
    void update(float seconds);
private:
    void sync_view();
    void refresh_scene();
    void focus_overview();
    NodeId current_=0;
    std::vector<NodeId> parents_;
    std::vector<unsigned char> levels_,scratch_levels_;
};
}
