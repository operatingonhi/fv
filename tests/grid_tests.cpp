#include "graphics/grid.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace spatial;
namespace {
void check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
bool near(double a,double b,double epsilon=0.0001) { return std::abs(a-b)<epsilon; }
void equivalent(const GridView& before,const GridView& after,Vec3 point,Vec3 translation,float scale) {
    check(near(after.spacing/before.spacing,scale),"Rebasing preserves grid spacing on screen");
    check(near(before.fine_opacity,after.fine_opacity),"Rebasing preserves refinement opacity");
    check(near(after.center.y,before.center.y*scale+translation.y),"Rebasing preserves the displayed floor plane");
    for (bool x:{true,false}) {
        const double p=x ? point.x:point.z;
        const double translated=p*scale+(x ? translation.x:translation.z);
        const double u=(p-(x ? before.center.x:before.center.z))/before.spacing-(x ? before.phase_x:before.phase_z);
        const double v=(translated-(x ? after.center.x:after.center.z))/after.spacing-(x ? after.phase_x:after.phase_z);
        check(std::abs(std::remainder(u-v,1.0))<0.0001,"Coarse grid phase survives coordinate changes");
        check(std::abs(std::remainder((u-v)*grid_refinement,1.0))<0.025,"Fine grid phase survives coordinate changes");
    }
}
void fades() {
    CameraState camera; camera.distance=24;
    const auto initial=grid_view({},camera);
    check(initial.spacing==1 && initial.fine_opacity==0,"Overview begins with the original grid spacing");
    float previous=-1;
    for (int i=0;i<100;++i) {
        camera.distance=static_cast<float>(24/std::pow(grid_refinement,i/100.0));
        const auto view=grid_view({},camera);
        check(view.fine_opacity>=previous,"Finer lines fade in monotonically as the camera approaches");
        check(view.fine_opacity>=0 && view.fine_opacity<=1,"Grid opacity remains bounded");
        previous=view.fine_opacity;
    }
    constexpr float child_scale=0.15555556f;
    camera.distance=24*std::pow(child_scale,3);
    check(near(grid_view({},camera).spacing,1/grid_refinement),"Next refinement is fully visible after three directory layers");
    for (int level=-3;level<=3;++level) {
        const float boundary=static_cast<float>(24/std::pow(grid_refinement,level));
        camera.distance=boundary*1.00001f; const auto before=grid_view({},camera);
        camera.distance=boundary*0.99999f; const auto after=grid_view({},camera);
        check(before.fine_opacity>0.9999f && after.fine_opacity<0.0001f,
            "At a level boundary the fully faded fine grid becomes the coarse grid");
        check(near(before.spacing/grid_refinement,after.spacing),"Refinement levels align without a spacing jump in visible lines");
        camera.distance=boundary*1.00001f; const auto reverse=grid_view({},camera);
        check(reverse.spacing==before.spacing && reverse.fine_opacity==before.fine_opacity,"Zoom reversal has no hysteresis or delayed fade");
    }
}
void frames() {
    GridFrame grid; CameraState camera;
    camera.focus={1.3f,0.5f,-3.1f}; camera.floor_height=0.5f; camera.distance=0.6f; camera.scale=0.025f;
    for (int i=0;i<80;++i) {
        const Vec3 translation{17.125f,-0.161f,31.375f}; constexpr float scale=265.67f;
        const auto before=grid_view(grid,camera);
        const auto point=camera.focus+Vec3{0.123f,0,-0.41f};
        grid=transform_grid(grid,translation,scale);
        CameraState incoming=camera;
        incoming.focus=incoming.focus*scale+translation;
        incoming.floor_height=incoming.floor_height*scale+translation.y;
        incoming.distance*=scale; incoming.scale*=scale;
        equivalent(before,grid_view(grid,incoming),point,translation,scale);
        // Settle at the next root and repeat. Local values stay bounded while
        // the persistent lattice covers hundreds of directory levels.
        camera.focus={1.3f,0.5f,-3.1f}; camera.floor_height=0.5f; camera.distance=0.6f; camera.scale=0.025f;
        check(grid.unit>=1 && grid.unit<grid_refinement && std::isfinite(grid.x),"Deep grid frames stay normalized");
    }
    for (int i=0;i<80;++i) {
        const Vec3 translation{-0.31f,0.1f,0.25f}; constexpr float scale=0.15555556f;
        const auto before=grid_view(grid,camera);
        const auto point=camera.focus;
        grid=transform_grid(grid,translation,scale);
        camera.focus=camera.focus*scale+translation;
        camera.floor_height=camera.floor_height*scale+translation.y;
        camera.distance*=scale; camera.scale*=scale;
        equivalent(before,grid_view(grid,camera),point,translation,scale);
        camera.distance=24; camera.scale=1;
    }
}
void floor_motion() {
    Camera camera; camera.focus({0,0,0},false,24,1,0);
    camera.focus({1,2,-3},true,24,1,1);
    camera.update(0.1f);
    const float height=camera.state().floor_height;
    check(height>0 && height<1,"Grid floor follows the explicit camera transition");
    camera.focus({0,4,0},true,24,1,0);
    check(camera.state().floor_height==height,"Interrupted floor animation starts at the displayed height");
    camera.update(1);
    check(camera.state().floor_height==0 && camera.state().focus.y==4,"Focusing a raised object does not lift the grid floor");
    camera.focus({0,2,0},false,24,1,1);
    check(camera.state().floor_height==1,"Disabled animation settles the grid floor immediately");
    const auto original=grid_view({},camera.state());
    camera.orbit(0.3f,0.1f);
    check(grid_view({},camera.state()).fine_opacity==original.fine_opacity,
        "Orbit does not trigger a grid refinement step");

}
}
int main() {
    try { fades(); frames(); floor_motion(); }
    catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
