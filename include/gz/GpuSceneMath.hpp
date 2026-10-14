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
    float sunR=1.f,sunG=.97f,sunB=.88f,shadowStrength=.36f;
    float fogR=.58f,fogG=.67f,fogB=.69f;
    float skyR=.44f,skyG=.59f,skyB=.69f;
    float wind=.35f,wetness=0.f,snowCover=0.f,cloudCover=.18f;
    bool firstPerson=false;
    float eyeHeight=1.65f,fov=56.f;
    bool waterReflections=true;
    bool enabled=false;
};
struct GpuFrameConstants {
    float viewProjection[16]{};
    float shadowProjection[16]{};
    float sun[4]{};      // xyz light direction; w ambient
    float fog[4]{};      // xyz fog color; w end distance
    float time[4]{};     // x seconds, y wind, z fog start, w direct light
    float eye[4]{};      // xyz camera position; w shadow footprint radius
    float sunColor[4]{}; // xyz linear light color; w shadow strength
    float sky[4]{};      // xyz sky color; w wetness
    float effects[4]{};  // snow cover, cloud cover, exposure, water reflections
    float terrainMap[4]{}; // origin X/Z, world extent, ready flag
    float viewport[4]{}; // width, height, shadow texel size, UNORM gamma encode
};
static_assert(sizeof(GpuFrameConstants)==272);
inline GpuFrameConstants makeGpuFrameConstants(const GpuSceneCamera& c,
    std::uint32_t width,std::uint32_t height,float seconds){
    const float d=std::clamp(c.distance,2.f,8000.f),p=std::clamp(c.pitch,c.firstPerson?-1.45f:.035f,c.firstPerson?1.45f:1.52f);
    const float sx=std::sin(c.yaw)*std::cos(p),sy=std::sin(p),sz=-std::cos(c.yaw)*std::cos(p);
    const float ex=c.firstPerson?c.focusX:c.focusX+sx*d,ey=c.firstPerson?c.focusY+c.eyeHeight:c.focusY+sy*d,ez=c.firstPerson?c.focusZ:c.focusZ+sz*d;
    // fwd points from eye to target, right = cross(fwd, world up).
    const float fx=-sx,fy=-sy,fz=-sz;
    const float rx=-fz/std::max(.001f,std::sqrt(fx*fx+fz*fz));
    const float rz=fx/std::max(.001f,std::sqrt(fx*fx+fz*fz));
    const float ux=-rz*fy,uy=rz*fx-rx*fz,uz=rx*fy;
    const float scaleY=1.f/std::tan(std::clamp(c.fov,35.f,100.f)*.00872664626f),scaleX=scaleY*float(height)/float(std::max(1u,width));
    const float nearZ=c.firstPerson?.045f:.15f,farZ=10000.f;
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
    out.fog[0]=c.fogR;out.fog[1]=c.fogG;out.fog[2]=c.fogB;out.fog[3]=c.fogEnd;
    out.time[0]=seconds;out.time[1]=c.wind;out.time[2]=c.fogStart;out.time[3]=c.direct;
    const float radius=c.distance<160.f?285.f:std::clamp(c.distance*1.05f+100.f,880.f,2000.f);
    out.eye[0]=ex;out.eye[1]=ey;out.eye[2]=ez;out.eye[3]=radius;
    out.sunColor[0]=c.sunR;out.sunColor[1]=c.sunG;out.sunColor[2]=c.sunB;out.sunColor[3]=c.shadowStrength;
    out.sky[0]=c.skyR;out.sky[1]=c.skyG;out.sky[2]=c.skyB;out.sky[3]=c.wetness;
    out.effects[0]=c.snowCover;out.effects[1]=c.cloudCover;out.effects[2]=1.05f;out.effects[3]=c.waterReflections?1.f:0.f;
    out.viewport[0]=float(width);out.viewport[1]=float(height);out.viewport[2]=1.f/2048.f;out.viewport[3]=1.f;
    // Orthographic light camera: a single bounded directional shadow map.
    float lx=c.sunX,ly=c.sunY,lz=c.sunZ;
    const float length=std::max(.001f,std::sqrt(lx*lx+ly*ly+lz*lz));lx/=length;ly/=length;lz/=length;
    const float horizontal=std::max(.001f,std::sqrt(lx*lx+lz*lz));
    const float srX=lz/horizontal,srZ=-lx/horizontal;
    const float suX=-srZ*ly,suY=srZ*lx-srX*lz,suZ=srX*ly;
    const float shadowRows[4][4]={
        {srX/radius,0.f,srZ/radius,-(srX*c.focusX+srZ*c.focusZ)/radius},
        {-suX/radius,-suY/radius,-suZ/radius,(suX*c.focusX+suY*c.focusY+suZ*c.focusZ)/radius},
        {-lx/8000.f,-ly/8000.f,-lz/8000.f,.5f+(lx*c.focusX+ly*c.focusY+lz*c.focusZ)/8000.f},
        {0.f,0.f,0.f,1.f}
    };
    for(int row=0;row<4;++row)for(int col=0;col<4;++col)out.shadowProjection[col*4+row]=shadowRows[row][col];
    return out;
}
}
