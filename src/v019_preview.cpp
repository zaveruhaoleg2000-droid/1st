#include "gz/Renderer.hpp"
#include <iostream>
int main(){
    gz::Simulation sim(4);
    sim.newOperation(96);
    sim.setMode(gz::ViewMode::Command);
    gz::Renderer r;
    r.resize(960,600);
    r.setTerrainDirectory("assets/terrain");
    r.loadTextures("assets/textures/runtime",128);
    r.setVisualTime(41.f);
    r.setShaderQuality(gz::ShaderQuality::Quality);
    gz::Camera c;
    c.focusX=3000.f;c.focusZ=1215.f;c.distance=170.f;c.pitch=.64f;c.yaw=.50f;
    r.draw(sim,c,0);
    if(!r.savePPM("preview_v019_battlefield.ppm"))return 1;
    std::cout<<"Battlefield triangles: "<<r.lastTriangles()<<"\n";
    sim.setMode(gz::ViewMode::Direct);
    c.focusX=3010.f;c.focusZ=1245.f;c.distance=320.f;c.pitch=.58f;c.yaw=.47f;
    r.draw(sim,c,0);
    if(!r.savePPM("preview_v019_village.ppm"))return 2;
    std::cout<<"Village triangles: "<<r.lastTriangles()<<"\n";
    gz::AtmosphereSettings sunset; sunset.time=gz::TimeOfDay::Sunset;
    r.setAtmosphere(sunset);
    c.focusX=3005.f;c.focusZ=1220.f;c.distance=200.f;c.pitch=.64f;c.yaw=.52f;
    r.draw(sim,c,0);
    if(!r.savePPM("preview_v019_sunset.ppm"))return 3;
    std::cout<<"Sunset triangles: "<<r.lastTriangles()<<"\n";
    return 0;
}
