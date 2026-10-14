#pragma once
#include "gz/GpuSceneMath.hpp"
#include "gz/Terrain.hpp"
#include <array>
#include <cmath>
namespace gz {
struct PickingRay {std::array<float,3> origin{},direction{};};
inline bool projectGpuPoint(const GpuFrameConstants& frame,int width,int height,float x,float y,float z,float& px,float& py){
    const auto& m=frame.viewProjection;
    const float w=m[3]*x+m[7]*y+m[11]*z+m[15];
    const float depth=m[2]*x+m[6]*y+m[10]*z+m[14];
    if(width<=0||height<=0||w<=.75f||depth<0.f||depth>w)return false;
    px=((m[0]*x+m[4]*y+m[8]*z+m[12])/w*.5f+.5f)*width;
    py=((m[1]*x+m[5]*y+m[9]*z+m[13])/w*.5f+.5f)*height;
    return std::isfinite(px)&&std::isfinite(py);
}
inline PickingRay gpuPickingRay(const GpuSceneCamera& c,int width,int height,float px,float py){
    const auto frame=makeGpuFrameConstants(c,std::uint32_t(std::max(width,1)),std::uint32_t(std::max(height,1)),0);
    const float pitch=std::clamp(c.pitch,.22f,1.4f);
    const float fx=-std::sin(c.yaw)*std::cos(pitch),fy=-std::sin(pitch),fz=std::cos(c.yaw)*std::cos(pitch);
    const float horizontal=std::sqrt(fx*fx+fz*fz),rx=-fz/horizontal,rz=fx/horizontal;
    const float ux=-rz*fy,uy=rz*fx-rx*fz,uz=rx*fy;
    const float nx=(2.f*px/std::max(width,1)-1.f)/(1.88f*float(std::max(height,1))/std::max(width,1));
    const float ny=(2.f*py/std::max(height,1)-1.f)/1.88f;
    const float dx=fx+rx*nx-ux*ny,dy=fy-uy*ny,dz=fz+rz*nx-uz*ny;
    const float length=std::sqrt(dx*dx+dy*dy+dz*dz);
    return {{frame.eye[0],frame.eye[1],frame.eye[2]},{dx/length,dy/length,dz/length}};
}
inline bool pickGpuTerrain(const GpuSceneCamera& camera,TerrainWorld& terrain,int width,int height,float px,float py,float& x,float& z){
    if(width<=0||height<=0||px<0||py<0||px>=width||py>=height)return false;
    const auto ray=gpuPickingRay(camera,width,height,px,py);
    if(ray.direction[1]>=-.001f)return false;
    auto altitude=[&](float t){return ray.origin[1]+ray.direction[1]*t-terrain.heightAt(ray.origin[0]+ray.direction[0]*t,ray.origin[2]+ray.direction[2]*t);};
    float previous=.75f;
    if(altitude(previous)<=0)return false;
    for(float t=2.f;t<=10000.f;t+=std::clamp(t*.025f,2.f,40.f)){
        if(altitude(t)<=0){
            float lo=previous,hi=t;
            for(int i=0;i<20;++i){float middle=(lo+hi)*.5f;if(altitude(middle)>0)lo=middle;else hi=middle;}
            x=ray.origin[0]+ray.direction[0]*hi;z=ray.origin[2]+ray.direction[2]*hi;return true;
        }
        previous=t;
    }
    return false;
}
}
