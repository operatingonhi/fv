#include "graphics/grid.hpp"
#include <algorithm>

namespace spatial {
GridFrame transform_grid(GridFrame grid,Vec3 translation,float scale) {
    if (!(scale>0) || !std::isfinite(scale)) return grid;
    grid.x=grid.x*scale+translation.x; grid.z=grid.z*scale+translation.z;
    grid.unit*=scale;
    // Powers of the subdivision ratio describe the same nested lattice. Keep
    // coordinates bounded so repeated deep rebasing does not lose line phase.
    while (grid.unit>=grid_refinement) grid.unit/=grid_refinement;
    while (grid.unit<1) grid.unit*=grid_refinement;
    const double period=grid.unit*grid_refinement*grid_refinement;
    grid.x=std::remainder(grid.x,period); grid.z=std::remainder(grid.z,period);
    return grid;
}
GridView grid_view(GridFrame grid,const CameraState& camera) {
    const double distance=std::max(0.00000001,static_cast<double>(camera.distance));
    const double zoom=std::log(24*grid.unit/distance)/std::log(grid_refinement);
    const double level=std::floor(zoom),fraction=zoom-level;
    const double spacing=grid.unit*std::pow(grid_refinement,-level);
    GridView result;
    result.center={camera.focus.x,camera.floor_height-0.02f*camera.scale,camera.focus.z};
    result.extent=camera.distance*12;
    result.spacing=static_cast<float>(spacing);
    result.fine_opacity=static_cast<float>(fraction*fraction*(3-2*fraction));
    // Subtract in double precision before uploading nearby, periodic coordinates.
    result.phase_x=static_cast<float>(std::remainder((grid.x-camera.focus.x)/spacing,1.0));
    result.phase_z=static_cast<float>(std::remainder((grid.z-camera.focus.z)/spacing,1.0));
    return result;
}
}
