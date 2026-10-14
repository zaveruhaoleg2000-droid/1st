#pragma once
#include "gz/Atmosphere.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace gz {
// Lightweight prototype event input: emitters are anchored to the demo village.
// y is a height ABOVE the local terrain until resolved by a renderer.
enum class LightKind : std::uint8_t { Fire, Headlamp, Explosion };
struct LocalLight {
    float x=0, y=0, z=0, radius=0;
    float r=1,g=1,b=1,energy=0;
    float forwardX=0, forwardZ=0; // headlamps point down +Z
    LightKind kind=LightKind::Fire;
};
struct ExplosionEvent {float x=3040.f,z=1320.f, start=-10000.f;};
inline float explosionPulse(float age) {
    if(age<0.f || age>1.25f)return 0.f;
    float attack=std::min(1.f,age*15.f);
    return attack*std::exp(-3.2f*age);
}
inline std::array<LocalLight,4> localLights(float visualTime,ExplosionEvent explosion={},bool enabled=true,AtmosphereSettings atmosphere={}){
    std::array<LocalLight,4> out{};
    if(!enabled)return out;
    const float fireFlicker=.82f+.18f*std::sin(visualTime*17.3f)+.08f*std::sin(visualTime*37.1f);
    out[0]={3090.f,3.2f,1334.f,39.f,1.f,.34f,.09f,1.75f*fireFlicker,0,0,LightKind::Fire};
    float headlightPower=atmosphere.time==TimeOfDay::Night?.65f:(atmosphere.time==TimeOfDay::Sunset?.30f:.04f);
    if(atmosphere.weather==Weather::Rain||atmosphere.weather==Weather::Storm||atmosphere.weather==Weather::Fog)
        headlightPower=std::max(headlightPower,.42f);
    out[1]={3053.7f,2.2f,1268.f,58.f,1.f,.93f,.67f,headlightPower,0.f,1.f,LightKind::Headlamp};
    out[2]={3057.3f,2.2f,1268.f,58.f,1.f,.93f,.67f,headlightPower,0.f,1.f,LightKind::Headlamp};
    const float pulse=explosionPulse(visualTime-explosion.start);
    out[3]={explosion.x,4.f,explosion.z,85.f,1.f,.48f,.15f,3.2f*pulse,0,0,LightKind::Explosion};
    return out;
}
// Same inexpensive attenuation model in CPU and HLSL GPU renderer.
inline float attenuation(const LocalLight& light,float x,float y,float z){
    if(light.energy<=0 || light.radius<=0)return 0.f;
    const float dx=x-light.x,dy=y-light.y,dz=z-light.z;
    const float sq=dx*dx+dy*dy+dz*dz;
    const float rr=light.radius*light.radius;
    if(sq>=rr)return 0.f;
    const float a=1.f-sq/rr;
    float cone=1.f;
    if(light.kind==LightKind::Headlamp){
        const float horiz=std::sqrt(dx*dx+dz*dz)+.0001f;
        const float angle=(dx*light.forwardX+dz*light.forwardZ)/horiz;
        cone=std::clamp((angle-.65f)*4.f,0.f,1.f);
    }
    return light.energy*a*a*cone;
}
inline float particleRandom(std::uint32_t n){
    n^=n>>16;n*=0x7feb352du;n^=n>>15;n*=0x846ca68bu;n^=n>>16;
    return float(n&0xffffffu)/float(0x1000000u);
}
struct PrecipitationParticle {float x=0,y=0,dx=0,dy=0;};
inline PrecipitationParticle precipitationParticle(int index,int w,int h,float t,Weather weather){
    const bool snow=weather==Weather::Snow;
    const float u=particleRandom(std::uint32_t(index)*747796405u+1123u);
    const float v=particleRandom(std::uint32_t(index)*277803737u+1973u);
    const float size=.55f+particleRandom(std::uint32_t(index)*189123u+91u);
    const float velocity=snow?(.075f+.037f*size):(.75f+.30f*size);
    const float drift=snow?(.035f+.010f*size):(.22f+.08f*size);
    const float ny=std::fmod(v+t*velocity,1.f);
    const float nx=std::fmod(u+t*drift+2.f,1.f);
    const float x=nx*w,y=ny*h;
    return {x,y,(snow?(-1.4f-.8f*size):(-4.f-4.f*size)),(snow?2.f+size:9.f+7.f*size)};
}
} // namespace gz
