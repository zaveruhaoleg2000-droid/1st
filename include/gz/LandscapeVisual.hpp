#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace gz {
// World-coordinate vegetation/soil masks: continuous across chunks and LODs.
// This is VISUAL data only; physics and elevation remain unchanged.
struct LandscapeSample { float soil; float meadow; };
inline float landscapeHash(int x,int z){
    std::uint32_t h=std::uint32_t(x)*374761393u+std::uint32_t(z)*668265263u+0x9e3779b9u;
    h=(h^(h>>13))*1274126177u;return float((h^(h>>16))&65535u)/65535.f;
}
inline float landscapeNoise(float x,float z,float scale){
    const float fx=x/scale,fz=z/scale;
    const int ix=int(std::floor(fx)),iz=int(std::floor(fz));
    float tx=fx-ix,tz=fz-iz;tx=tx*tx*(3.f-2.f*tx);tz=tz*tz*(3.f-2.f*tz);
    float a=landscapeHash(ix,iz),b=landscapeHash(ix+1,iz);
    float c=landscapeHash(ix,iz+1),d=landscapeHash(ix+1,iz+1);
    return ((a+(b-a)*tx)*(1-tz)+(c+(d-c)*tx)*tz);
}
inline LandscapeSample landscapeAt(float x,float z){
    const float large=landscapeNoise(x,z,155.f);
    const float small=landscapeNoise(x,z,52.f);
    const float n=.74f*large+.26f*small;
    return {std::clamp((n-.48f)*2.2f,0.f,.82f),
            std::clamp((.52f-n)*2.1f+.22f,0.f,.72f)};
}
inline float roadCenterHorizontal(float x){return 1200.f+30.f*std::sin(x*.001f);}
inline float roadCenterVertical(float z){return 3000.f+35.f*std::sin(z*.0008f);}
}
