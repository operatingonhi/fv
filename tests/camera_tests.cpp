#include "graphics/camera.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
void check(bool result,const char* description) {
    if (!result) { std::cerr<<description<<'\n'; std::exit(1); }
}
bool near(float a,float b) { return std::abs(a-b)<0.0001f; }
}
int main() {
    using namespace spatial;
    Camera camera;
    camera.orbit(100,100);
    check(near(camera.state().yaw,Camera::max_yaw) && near(camera.state().pitch,Camera::max_pitch),"Upper orbit limits");
    camera.orbit(-100,-100);
    check(near(camera.state().yaw,-Camera::max_yaw) && near(camera.state().pitch,Camera::min_pitch),"Lower orbit limits");
    camera.zoom(10000);
    check(near(camera.state().distance,Camera::min_distance),"Minimum zoom");
    camera.zoom(-10000);
    check(near(camera.state().distance,Camera::max_distance),"Maximum zoom");
    camera.reset(false);
    camera.focus({4,0,-3});
    camera.update(-1);
    check(near(camera.state().focus.x,0),"Negative timestep must not reverse animation");
    camera.update(0.2f);
    check(camera.moving() && near(camera.state().focus.x,2),"Transition midpoint");
    const Vec3 midpoint=camera.state().focus;
    camera.focus({-4,0,0});
    check(near(camera.state().focus.x,midpoint.x),"Retarget without a jump");
    camera.update(1);
    check(!camera.moving() && near(camera.state().focus.x,-4),"Transition finishes at target");
    camera.focus({4,0,0});
    camera.update(0.1f);
    const auto before_orbit=camera.state();
    camera.orbit(0,0); // A right-button drag may begin with no movement.
    camera.orbit(0.1f,0.2f);
    check(camera.moving() && near(camera.state().focus.x,before_orbit.focus.x) &&
        near(camera.state().yaw,before_orbit.yaw) && near(camera.state().pitch,before_orbit.pitch),
        "Orbit input leaves active transition unchanged");
    camera.update(1);
    check(!camera.moving() && near(camera.state().focus.x,4) && near(camera.state().yaw,0),
        "Transition reaches destination despite orbit input");
    camera.orbit(0.1f,0.2f);
    check(near(camera.state().yaw,0.1f) && near(camera.state().pitch,0.75f),
        "Orbit works after transition finishes");
    camera.focus({4,0,0}); camera.zoom(1);
    check(!camera.moving(),"Zoom interrupts transition");
    camera.reset();
    camera.orbit(0.4f,0.4f);
    check(camera.moving(),"Reset transition takes priority over orbit");
    camera.update(1);
    check(near(camera.state().focus.x,0) && near(camera.state().distance,18),"Reset returns to overview");
    Camera a,b;
    a.focus({4,0,-3}); b.focus({4,0,-3});
    a.update(0.2f); b.update(0.1f); b.update(0.1f);
    check(near(a.state().focus.x,b.state().focus.x),"Transition independent of timestep partition");
    camera.focus({4,0,-3},false);
    const auto matrix=camera.view_projection(16.0f/9.0f);
    const auto focus=camera.state().focus;
    const float x=matrix[0]*focus.x+matrix[4]*focus.y+matrix[8]*focus.z+matrix[12];
    const float y=matrix[1]*focus.x+matrix[5]*focus.y+matrix[9]*focus.z+matrix[13];
    check(near(x,0) && near(y,0),"Focal point projects to screen center");
    for (float value:matrix) check(std::isfinite(value),"Finite projection matrix");
    const auto portrait=camera.view_projection(0.5f);
    check(portrait[0]>matrix[0],"Projection responds to drawable aspect ratio");
    camera.orbit(100,100);
    for (float value:camera.view_projection(1))
        check(std::isfinite(value),"Finite matrix at orbit limits");
    const auto eye=camera.eye();
    check(near(std::sqrt(dot(eye-focus,eye-focus)),camera.state().distance),"Eye stays on orbit radius");
    camera.focus({},false,1000);
    check(near(camera.state().distance,Camera::max_distance),"Requested focus distance has upper bound");
    camera.focus({},false,0);
    check(near(camera.state().distance,Camera::min_distance),"Requested focus distance has lower bound");
    camera.focus({2,0,-1},false,0.24f,0.01f);
    check(near(camera.state().distance,0.24f),"Nested browsing distance is not clamped to root-scale minimum");
    camera.zoom(10000);
    check(near(camera.state().distance,Camera::min_distance*0.01f),"Zoom minimum follows nested scale");
    camera.zoom(-10000);
    check(near(camera.state().distance,Camera::max_distance*0.01f),"Zoom maximum follows nested scale");
    camera.orbit(100,100);
    check(near(camera.state().yaw,Camera::max_yaw) && near(camera.state().pitch,Camera::max_pitch),"Nested orbit retains existing angular limits");
    for (float value:camera.view_projection(1)) check(std::isfinite(value),"Nested clipping produces a finite matrix");
    camera.focus({},true,24,1);
    const float start_distance=camera.state().distance;
    camera.update(0.2f);
    check(camera.state().distance>start_distance && camera.state().distance<24 &&
        camera.state().scale>0.01f && camera.state().scale<1,"Parent navigation interpolates scale and distance");
    std::cout<<"Camera tests passed\n";
}
