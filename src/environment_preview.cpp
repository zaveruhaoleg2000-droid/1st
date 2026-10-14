#include "gz/Renderer.hpp"
#include <iostream>
#include <string>
int main(int argc,char** argv){
    const std::string prefix=argc>1?argv[1]:"environment";
    gz::Simulation sim(4);sim.newOperation(36);sim.step(40);
    gz::Renderer r;r.resize(1152,680);r.setTerrainDirectory("assets/terrain");
    r.loadTextures("assets/textures/runtime",256);
    gz::Camera c;c.focusX=3080;c.focusZ=1330;c.distance=147;c.pitch=.58f;c.yaw=.45f;
    struct Shot{const char* name;gz::AtmosphereSettings atmosphere;float time;bool lit;bool particles;bool boom;};
    const Shot shots[]={
       {"night_rain",{gz::Weather::Rain,gz::TimeOfDay::Night,true},6.4f,true,true,false},
       {"night_no_lights",{gz::Weather::Rain,gz::TimeOfDay::Night,true},6.4f,false,true,false},
       {"snow",{gz::Weather::Snow,gz::TimeOfDay::Morning,true},3.4f,true,true,false},
       {"sunset_explosion",{gz::Weather::Clear,gz::TimeOfDay::Sunset,true},3.12f,true,false,true},
       {"sunset",{gz::Weather::Clear,gz::TimeOfDay::Sunset,true},3.12f,true,false,false},
       {"storm",{gz::Weather::Storm,gz::TimeOfDay::Sunset,true},8.2f,true,true,false}
    };
    for(const auto& shot:shots){
        r.setAtmosphere(shot.atmosphere);r.setVisualTime(shot.time);
        r.setDynamicLights(shot.lit);r.setPrecipitation(shot.particles);
        r.triggerExplosion(3090,1334,shot.boom?3.f:-10000.f);
        r.draw(sim,c,0);
        if(!r.savePPM(prefix+"_"+shot.name+".ppm")){std::cerr<<"cannot save output\n";return 1;}
        std::cout<<shot.name<<" triangles "<<r.lastTriangles()<<"\n";
    }
    if(argc>2 && std::string(argv[2])=="--animation"){
        r.resize(800,480);
        r.setAtmosphere({gz::Weather::Rain,gz::TimeOfDay::Night,true});
        r.setDynamicLights(true);r.setPrecipitation(true);
        r.triggerExplosion(3090,1334,-10000.f);
        for(int i=0;i<12;++i){
            r.setVisualTime(4.f+float(i)*.16f);
            r.draw(sim,c,0);
            r.savePPM(prefix+"_anim_"+std::to_string(i)+".ppm");
        }
    }
}
