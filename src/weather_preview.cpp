#include "gz/Renderer.hpp"
#include <iostream>
#include <string>
int main(int argc,char** argv){
    const std::string prefix=argc>1?argv[1]:"weather";
    gz::Simulation sim(4);sim.newOperation(64);sim.step(40);
    gz::Renderer r;r.resize(1100,680);
    r.setTerrainDirectory("assets/terrain");
    r.loadTextures("assets/textures/runtime",256);
    gz::Camera camera;
    camera.focusX=3070;camera.focusZ=1380;
    camera.distance=560;camera.pitch=.67f;camera.yaw=.45f;
    r.setVisualTime(6.5f);
    struct Shot{const char* name;gz::AtmosphereSettings atm;};
    Shot shots[]={{"sunny",{}},{"sunset",{gz::Weather::Clear,gz::TimeOfDay::Sunset,true}},
                  {"fog",{gz::Weather::Fog,gz::TimeOfDay::Morning,true}},
                  {"storm",{gz::Weather::Storm,gz::TimeOfDay::Noon,true}},
                  {"night",{gz::Weather::Clear,gz::TimeOfDay::Night,true}},
                  {"no_shadows",{gz::Weather::Clear,gz::TimeOfDay::Noon,false}}};
    for(const auto& s:shots){
        r.setAtmosphere(s.atm);r.draw(sim,camera,0);
        const std::string out=prefix+"_"+s.name+".ppm";
        if(!r.savePPM(out)){std::cerr<<"Cannot save "<<out<<"\n";return 1;}
        std::cout<<out<<" | tris "<<r.lastTriangles()<<" | visibility "
                 <<gz::atmosphereProfile(s.atm).visibilityMeters<<" m\n";
    }
    // Near-ground inspection exposes actual 3D conifer geometry and cast shadows.
    camera.focusX=3100;camera.focusZ=1360;camera.distance=132;camera.pitch=.52f;
    r.setAtmosphere({gz::Weather::Clear,gz::TimeOfDay::Sunset,true});
    r.draw(sim,camera,0);
    if(!r.savePPM(prefix+"_close.ppm"))return 2;
}
