#pragma once
#include "graphics/camera.hpp"

namespace spatial {
// A persistent lattice, independent of the filesystem and scene geometry cache.
struct GridFrame { double x=0,z=0,unit=1; bool operator==(const GridFrame&) const = default; };
struct GridView {
    Vec3 center{};
    float extent=1,spacing=1,fine_opacity=0;
    float phase_x=0,phase_z=0; // Phase relative to center, in coarse cells.
};
inline constexpr double grid_refinement=256;
GridFrame transform_grid(GridFrame grid,Vec3 translation,float scale);
GridView grid_view(GridFrame grid,const CameraState& camera);
}
