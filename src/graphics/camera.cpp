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
    current_.distance=std::clamp(current_.distance*std::exp(-steps*0.12f),min_distance*current_.scale,max_distance*current_.scale);
}
void Camera::transition(CameraState target, bool animate) {
    start_=current_;
    target_=target;
    elapsed_=0;
    duration_=animate ? 0.4f : 0.0f;
    if (!animate) current_=target;
}
void Camera::focus(Vec3 target, bool animate, float distance, float scale) {
    CameraState next;
    next.focus=target;
    next.scale=std::max(scale,0.000001f);
    next.distance=std::clamp(distance,min_distance*next.scale,max_distance*next.scale);
    transition(next,animate);
}
void Camera::restore(CameraState state,bool animate) {
    state.scale=std::max(state.scale,0.000001f);
    state.distance=std::clamp(state.distance,min_distance*state.scale,max_distance*state.scale);
    state.yaw=std::clamp(state.yaw,-max_yaw,max_yaw); state.pitch=std::clamp(state.pitch,min_pitch,max_pitch);
    transition(state,animate);
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
    current_.scale=mix(start_.scale,target_.scale);
}
Vec3 Camera::eye() const {
    const float horizontal=current_.distance*std::cos(current_.pitch);
    return current_.focus+Vec3{horizontal*std::sin(current_.yaw),
        current_.distance*std::sin(current_.pitch),horizontal*std::cos(current_.yaw)};
}
Mat4 Camera::view_projection(float aspect) const {
    return perspective(aspect,current_.distance*0.002f,current_.distance*50.0f)*look_at(eye(),current_.focus);
}
}
