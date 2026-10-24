#pragma once
#include "graphics/math.hpp"

namespace spatial {
struct CameraState {
    Vec3 focus{};
    float yaw=0.0f, pitch=0.55f, distance=18.0f;
};
class Camera {
public:
    static constexpr float min_pitch=0.20f, max_pitch=1.15f;
    static constexpr float max_yaw=0.85f, min_distance=5.0f, max_distance=32.0f;
    const CameraState& state() const { return current_; }
    bool moving() const { return elapsed_ < duration_; }
    void orbit(float yaw_delta, float pitch_delta);
    void zoom(float steps);
    void focus(Vec3 target, bool animate=true);
    void reset(bool animate=true);
    void update(float seconds);
    Vec3 eye() const;
    Mat4 view_projection(float aspect) const;
private:
    void transition(CameraState target, bool animate);
    CameraState current_{}, start_{}, target_{};
    float elapsed_=0.0f, duration_=0.0f;
};
}
