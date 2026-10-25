#pragma once
#include <array>
#include <cmath>

namespace spatial {
struct Vec3 { float x{}, y{}, z{}; bool operator==(const Vec3&) const = default; };
inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
inline Vec3 operator*(Vec3 a, float s) { return {a.x*s, a.y*s, a.z*s}; }
inline float dot(Vec3 a, Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};
}
inline Vec3 normalized(Vec3 a) { return a*(1.0f/std::sqrt(dot(a,a))); }
using Mat4 = std::array<float, 16>; // Column-major, column vectors (OpenGL).
inline Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 result{};
    for (int c=0; c<4; ++c)
        for (int r=0; r<4; ++r)
            for (int k=0; k<4; ++k)
                result[c*4+r] += a[k*4+r]*b[c*4+k];
    return result;
}
inline Mat4 perspective(float aspect,float near=0.1f,float far=150.0f) {
    const float f=1.0f/std::tan(0.78539816f/2.0f);
    return {f/aspect,0,0,0, 0,f,0,0, 0,0,(far+near)/(near-far),-1,
            0,0,2*far*near/(near-far),0};
}
inline Mat4 look_at(Vec3 eye, Vec3 target) {
    const Vec3 f=normalized(target-eye), s=normalized(cross(f,{0,1,0})), u=cross(s,f);
    return {s.x,u.x,-f.x,0, s.y,u.y,-f.y,0, s.z,u.z,-f.z,0,
            -dot(s,eye),-dot(u,eye),dot(f,eye),1};
}
}
