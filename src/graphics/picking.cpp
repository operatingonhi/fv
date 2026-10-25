#include "graphics/picking.hpp"
#include <algorithm>
#include <limits>

namespace spatial {
std::optional<ScreenPoint> project(Vec3 p,const Mat4& m,float width,float height) {
    const float w=m[3]*p.x+m[7]*p.y+m[11]*p.z+m[15];
    if (w<=0 || width<=0 || height<=0) return {};
    const float x=(m[0]*p.x+m[4]*p.y+m[8]*p.z+m[12])/w;
    const float y=(m[1]*p.x+m[5]*p.y+m[9]*p.z+m[13])/w;
    const float z=(m[2]*p.x+m[6]*p.y+m[10]*p.z+m[14])/w;
    if (z<-1 || z>1) return {};
    return ScreenPoint{(x+1)*width/2,(1-y)*height/2,z};
}
std::optional<std::size_t> pick(std::span<const SceneBox> boxes,const Camera& camera,
    float x,float y,float width,float height) {
    if (width<=0 || height<=0 || x<0 || y<0 || x>=width || y>=height) return {};
    const auto eye=camera.eye(), forward=normalized(camera.state().focus-eye);
    const auto right=normalized(cross(forward,{0,1,0})), up=cross(right,forward);
    const float tangent=std::tan(0.78539816f/2);
    const auto direction=normalized(forward+right*((2*x/width-1)*tangent*width/height)+
        up*((1-2*y/height)*tangent));
    std::optional<std::size_t> hit;
    float closest=std::numeric_limits<float>::infinity();
    for (std::size_t i=0;i<boxes.size();++i) {
        const auto low=boxes[i].center-boxes[i].size*0.5f, high=boxes[i].center+boxes[i].size*0.5f;
        const float origins[]={eye.x,eye.y,eye.z}, dirs[]={direction.x,direction.y,direction.z};
        const float mins[]={low.x,low.y,low.z}, maxs[]={high.x,high.y,high.z};
        float enter=0,leave=closest;
        bool intersects=true;
        for (int axis=0;axis<3;++axis) {
            if (std::abs(dirs[axis])<0.000001f) {
                if (origins[axis]<mins[axis] || origins[axis]>maxs[axis]) intersects=false;
            } else {
                float a=(mins[axis]-origins[axis])/dirs[axis], b=(maxs[axis]-origins[axis])/dirs[axis];
                if (a>b) std::swap(a,b);
                enter=std::max(enter,a); leave=std::min(leave,b);
                if (enter>leave) intersects=false;
            }
        }
        if (intersects && enter<closest) { closest=enter; hit=i; }
    }
    return hit;
}
}
