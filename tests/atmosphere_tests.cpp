#include "gz/Atmosphere.hpp"
#include "gz/Renderer.hpp"
#include "gz/SceneMesh.hpp"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
#define CHECK(x) do{ if(!(x))throw std::runtime_error("CHECK failed: " #x); }while(false)
namespace {
std::size_t changed(const std::vector<std::uint32_t>& a,const std::vector<std::uint32_t>& b){
    std::size_t n=0;for(std::size_t i=0;i<a.size();++i)n+=a[i]!=b[i];return n;
}
}
int main(){
    using namespace gz;
    AtmosphereSettings clear, mist, night, shadowsOff;
    mist.weather=Weather::Fog;night.time=TimeOfDay::Night;shadowsOff.shadows=false;
    auto c=atmosphereProfile(clear),f=atmosphereProfile(mist),n=atmosphereProfile(night);
    CHECK(c.visibilityMeters>f.visibilityMeters);
    CHECK(c.fogFar>f.fogFar && c.sunY>0.f);
    CHECK(n.skyR<c.skyR && n.ambient<c.ambient);
    CHECK(atmosphereProfile(shadowsOff).shadowOpacity==0.f);
    CHECK(atmosphereProfile({Weather::Storm,TimeOfDay::Noon,true}).windMultiplier>2.f);
    // Sight distance changes actual prototype combat engagement, not only visuals.
    Simulation normal(4),foggy(4);
    normal.newOperation(8);foggy.newOperation(8);
    normal.orderSquad(0,3300,1100);foggy.orderSquad(0,3300,1100);
    normal.setVisibilityMeters(c.visibilityMeters);
    foggy.setVisibilityMeters(f.visibilityMeters);
    normal.step(700);foggy.step(700);
    CHECK(normal.stats().shots>foggy.stats().shots);
    CHECK(normal.stats().shots>0);
    Renderer renderer;renderer.resize(550,340);
    renderer.setTerrainDirectory("assets/terrain");
    renderer.loadTextures("assets/textures/runtime",128);
    Camera cam;cam.focusX=3000;cam.focusZ=1300;cam.distance=560;cam.pitch=.66f;
    auto frame=[&](AtmosphereSettings atm){
        renderer.setAtmosphere(atm);renderer.setVisualTime(7.f);
        renderer.draw(normal,cam,0);
        return std::vector<std::uint32_t>(renderer.data(),renderer.data()+renderer.width()*renderer.height());
    };
    auto base=frame(clear);
    CHECK(changed(base,frame(clear))==0);
    auto dark=frame(night);CHECK(changed(base,dark)>20000);
    auto fog=frame(mist);CHECK(changed(base,fog)>20000);
    auto noShadow=frame(shadowsOff);CHECK(changed(base,noShadow)>60);
    TerrainWorld world{9};world.setTileDirectory("assets/terrain");
    SceneMesh mesh;SceneMeshBuilder{}.build(mesh,world,normal,cam,0,clear);
    CHECK(mesh.shadowIndexCount>0 && mesh.shadowCasterCount>0);
    CHECK(mesh.shadowFirstIndex+mesh.shadowIndexCount==mesh.indices.size());
    CHECK(mesh.shadowIndexCount==mesh.shadowCasterCount*6);
    SceneMesh without;SceneMeshBuilder{}.build(without,world,normal,cam,0,shadowsOff);
    CHECK(without.shadowIndexCount==0 && without.shadowCasterCount==0);
    CHECK(mesh.indices.size()>without.indices.size());
    std::cout<<"ATMOSPHERE PASS | visibility "<<c.visibilityMeters<<"m clear / "<<f.visibilityMeters
             <<"m fog | clear shots "<<normal.stats().shots<<" vs fog shots "<<foggy.stats().shots
             <<" | shadow casters "<<mesh.shadowCasterCount<<" | fog/clear pixels "<<changed(base,fog)<<"\n";
}
