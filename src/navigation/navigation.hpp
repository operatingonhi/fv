#pragma once
#include "app/mock_scene.hpp"
#include <cstddef>

namespace spatial {
// Navigation exposes a semantic focus request; camera interpolation lives elsewhere.
class Navigation {
public:
    std::size_t selected() const { return selected_; }
    const MockDirectory& directory() const { return directories[selected_]; }
    Vec3 focus_next() { selected_=(selected_+1)%directories.size(); return directory().center; }
    Vec3 focus(std::size_t index) { selected_=index%directories.size(); return directory().center; }
private:
    std::size_t selected_=0;
};
}
