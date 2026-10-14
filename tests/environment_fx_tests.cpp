#include "gz/Renderer.hpp"
#include "gz/EnvironmentFX.hpp"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
#define CHECK(cond) do{if(!(cond))throw std::runtime_error("FAILED: " #cond);}while(false)
namespace {
std::size_t changed(const std::vector<std::uint32_t>&a,const std::vector<std::uint32_t>&b){
    std::size_t n=0;for(std::size_t i=0;i<a.size();++i)n+=(a[i]!=b[i]);return n;
}
}
int main(){
    using namespace gz;
    auto lights=localLights(2.0f);
    CHECK(lights[0].kind==LightKind::Fire && lights[0].energy>1.f);
    CHECK(attenuation(lights[0],lights[0].x,lights[0].y,lights[0].z)>0.1f);
    CHECK(attenuation(lights[0],lights[0].x+lights[0].radius*2.f,lights[0].y,lights[0].z)==0.f);
    CHECK(attenuation(lights[1],lights[1].x,lights[1].y,lights[1].z+18.f)>
          attenuation(lights[1],lights[1].x,lights[1].y,lights[1].z-18.f));
    CHECK(explosionPulse(0.12f)>explosionPulse(.85f));
    CHECK(explosionPulse(-.01f)==0.f && explosionPulse(1.26f)==0.f);
    CHECK(localLights(6.f,{3090,1334,5.9f})[3].energy>0.f);
    CHECK(localLights(6.f,{3090,1334,4.f})[3].energy==0.f);
    CHECK(localLights(2.f,{},false)[0].energy==0.f);
    auto snow=atmosphereProfile({Weather::Snow,TimeOfDay::Noon,true});
    auto rain=atmosphereProfile({Weather::Rain,TimeOfDay::Noon,true});
    CHECK(snow.snowCover>.5f && snow.visibilityMeters<220.f);
    CHECK(rain.wetness>.5f && rain.snowCover==0.f);
    auto pos=precipitationParticle(17,600,360,2.f,Weather::Rain);
    CHECK(pos.x>=0.f && pos.x<600.f && pos.y>=0.f && pos.y<360.f);
    auto pos2=precipitationParticle(17,600,360,2.f,Weather::Rain);
    CHECK(pos.x==pos2.x && pos.y==pos2.y);
    Simulation sim(4);sim.newOperation(16);
    Renderer render;render.resize(610,365);render.setTerrainDirectory("assets/terrain");
    render.loadTextures("assets/textures/runtime",128);
    Camera cam;cam.focusX=3080.f;cam.focusZ=1330.f;cam.distance=147.f;cam.pitch=.58f;cam.yaw=.45f;
    auto frame=[&](AtmosphereSettings atmosphere,float time){
        render.setAtmosphere(atmosphere);render.setVisualTime(time);render.draw(sim,cam,0);
        return std::vector<std::uint32_t>(render.data(),render.data()+render.width()*render.height());
    };
    const AtmosphereSettings clear{Weather::Clear,TimeOfDay::Sunset,true};
    const auto lit=frame(clear,2.f);
    CHECK(changed(lit,frame(clear,2.f))==0);
    render.setDynamicLights(false);
    const auto noLight=frame(clear,2.f);CHECK(changed(lit,noLight)>100);
    render.setDynamicLights(true);
    render.triggerExplosion(3090,1334,2.f);
    const auto boom=frame(clear,2.12f);
    render.triggerExplosion(3090,1334,-100.f);
    const auto noBoom=frame(clear,2.12f);CHECK(changed(boom,noBoom)>100);
    const AtmosphereSettings raining{Weather::Rain,TimeOfDay::Sunset,true};
    render.setPrecipitation(false);
    const auto dryParticle=frame(raining,2.f);
    render.setPrecipitation(true);
    const auto rainy=frame(raining,2.f);
    CHECK(changed(dryParticle,rainy)>350);
    CHECK(changed(rainy,frame(raining,2.f))==0);
    const AtmosphereSettings snowy{Weather::Snow,TimeOfDay::Sunset,true};
    render.setPrecipitation(false);const auto snowStatic=frame(snowy,2.f);
    render.setPrecipitation(true);const auto snowing=frame(snowy,2.f);
    CHECK(changed(snowStatic,snowing)>250);
    CHECK(changed(snowing,lit)>12000);
    std::cout<<"ENVIRONMENT FX PASS | lights "<<changed(lit,noLight)
             <<" | explosion "<<changed(boom,noBoom)
             <<" | rain "<<changed(rainy,dryParticle)
             <<" | snow "<<changed(snowing,snowStatic)<<"\n";
}
