#include "graphics/camera.hpp"
#include "navigation/navigation.hpp"
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
    camera.orbit(0.1f,0);
    const float interrupted=camera.state().focus.x;
    camera.update(1);
    check(!camera.moving() && near(camera.state().focus.x,interrupted),"Orbit interrupts transition");
    camera.focus({4,0,0}); camera.zoom(1);
    check(!camera.moving(),"Zoom interrupts transition");
    camera.reset(); camera.update(1);
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
    Navigation navigation;
    navigation.focus_next();
    check(navigation.selected()==1,"Next mock directory");
    navigation.focus_next();
    check(navigation.selected()==0,"Mock focus wraps");
    std::cout<<"Camera and mock navigation tests passed\n";
}
