#include "gz/Renderer.hpp"
#include "gz/Shaders.hpp"
#include <iostream>
#include <string>
int main(int argc,char**argv){
    const std::string prefix=argc>1?argv[1]:"shader_v012";
    gz::ShaderLibrary library;
    if(!library.load("assets/shaders")){std::cerr<<library.error()<<'\n';return 2;}
    gz::Simulation sim(4);sim.newOperation(36);sim.step(40);
    gz::Renderer renderer;renderer.resize(1152,680);renderer.setTerrainDirectory("assets/terrain");
    renderer.loadTextures("assets/textures/runtime",256);
    gz::Camera cam;cam.focusX=3080;cam.focusZ=1330;cam.distance=147;cam.pitch=.58f;cam.yaw=.45f;
    renderer.setAtmosphere({gz::Weather::Rain,gz::TimeOfDay::Sunset,true});
    renderer.setVisualTime(6.4f);renderer.setDynamicLights(true);renderer.setPrecipitation(false);
    renderer.triggerExplosion(3090.f,1334.f,-10000.f);
    for(int q=0;q<3;++q){
        const auto quality=gz::shaderQualityFromInt(q);
        renderer.setShaderQuality(quality);
        renderer.draw(sim,cam,0);
        const std::string filename=prefix+"_"+gz::shaderQualityName(quality)+".ppm";
        if(!renderer.savePPM(filename))return 1;
        std::cout<<filename<<"  "<<renderer.lastTriangles()<<" triangles\n";
    }
    return 0;
}
