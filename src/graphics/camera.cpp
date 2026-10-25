#include "graphics/camera.hpp"
#include <algorithm>

namespace spatial {
void Camera::orbit(float yaw_delta, float pitch_delta) {
    if (moving()) return; // Finish navigation before accepting orbit input.
    current_.yaw=std::clamp(current_.yaw+yaw_delta,-max_yaw,max_yaw);
    current_.pitch=std::clamp(current_.pitch+pitch_delta,min_pitch,max_pitch);
}
void Camera::zoom(float steps) {
    duration_=elapsed_=0;
    current_.distance=std::clamp(current_.distance*std::exp(-steps*0.12f),min_distance,max_distance);
}
void Camera::transition(CameraState target, bool animate) {
    start_=current_;
    target_=target;
    elapsed_=0;
    duration_=animate ? 0.4f : 0.0f;
    if (!animate) current_=target;
}
void Camera::focus(Vec3 target, bool animate, float distance) {
    CameraState next;
    next.focus=target;
    next.distance=std::clamp(distance,min_distance,max_distance);
    transition(next,animate);
}
void Camera::reset(bool animate) { transition(CameraState{},animate); }
void Camera::update(float seconds) {
    if (!moving()) return;
    elapsed_=std::min(duration_,elapsed_+std::max(seconds,0.0f));
    const float t=elapsed_/duration_, eased=t*t*(3-2*t);
    const auto mix=[eased](float a,float b) { return a+(b-a)*eased; };
    current_.focus=start_.focus+(target_.focus-start_.focus)*eased;
    current_.yaw=mix(start_.yaw,target_.yaw);
    current_.pitch=mix(start_.pitch,target_.pitch);
    current_.distance=mix(start_.distance,target_.distance);
}
Vec3 Camera::eye() const {
    const float horizontal=current_.distance*std::cos(current_.pitch);
    return current_.focus+Vec3{horizontal*std::sin(current_.yaw),
        current_.distance*std::sin(current_.pitch),horizontal*std::cos(current_.yaw)};
}
Mat4 Camera::view_projection(float aspect) const {
    return perspective(aspect)*look_at(eye(),current_.focus);
}
}
