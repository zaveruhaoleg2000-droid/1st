#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace gz {
// Plain-data camera packet crossing the platform/rendering boundary.
// Matrices are column-major, Vulkan clip Z is [0,1], viewport Y points down.
struct GpuSceneCamera {
    float focusX=3000.f,focusY=0.f,focusZ=1200.f;
    float distance=550.f,pitch=.6f,yaw=.35f;
    float sunX=.35f,sunY=.86f,sunZ=.27f;
    float ambient=.34f, direct=.66f, fogStart=650.f,fogEnd=2200.f;
    bool enabled=false;
};
struct GpuFrameConstants {
    float viewProjection[16]{};
    float sun[4]{};      // xyz light direction; w ambient
    float fog[4]{};      // xyz fog color; w end distance
    float time[4]{};     // x seconds, y wind, z fog start, w direct light
};
static_assert(sizeof(GpuFrameConstants)==112);
inline GpuFrameConstants makeGpuFrameConstants(const GpuSceneCamera& c,
    std::uint32_t width,std::uint32_t height,float seconds){
    const float d=std::clamp(c.distance,10.f,4000.f),p=std::clamp(c.pitch,.22f,1.4f);
    const float sx=std::sin(c.yaw)*std::cos(p),sy=std::sin(p),sz=-std::cos(c.yaw)*std::cos(p);
    const float ex=c.focusX+sx*d,ey=c.focusY+sy*d,ez=c.focusZ+sz*d;
    // fwd points from eye to target, right = cross(fwd, world up).
    const float fx=-sx,fy=-sy,fz=-sz;
    const float rx=-fz/std::max(.001f,std::sqrt(fx*fx+fz*fz));
    const float rz=fx/std::max(.001f,std::sqrt(fx*fx+fz*fz));
    const float ux=-rz*fy,uy=rz*fx-rx*fz,uz=rx*fy;
    const float scaleY=1.88f,scaleX=scaleY*float(height)/float(std::max(1u,width));
    const float nearZ=.75f,farZ=10000.f;
    const float A=farZ/(farZ-nearZ),B=nearZ*farZ/(farZ-nearZ);
    const float rows[4][4]={
        {rx*scaleX,0.f,rz*scaleX,-(rx*ex+rz*ez)*scaleX},
        {-ux*scaleY,-uy*scaleY,-uz*scaleY,(ux*ex+uy*ey+uz*ez)*scaleY},
        {fx*A,fy*A,fz*A,-(fx*ex+fy*ey+fz*ez)*A-B},
        {fx,fy,fz,-(fx*ex+fy*ey+fz*ez)}
    };
    GpuFrameConstants out{};
    for(int row=0;row<4;++row)for(int col=0;col<4;++col)
        out.viewProjection[col*4+row]=rows[row][col];
    out.sun[0]=c.sunX;out.sun[1]=c.sunY;out.sun[2]=c.sunZ;
    out.sun[3]=c.ambient;
    out.fog[0]=.49f;out.fog[1]=.58f;out.fog[2]=.62f;out.fog[3]=c.fogEnd;
    out.time[0]=seconds;out.time[1]=.35f;out.time[2]=c.fogStart;out.time[3]=c.direct;
    return out;
}
}
