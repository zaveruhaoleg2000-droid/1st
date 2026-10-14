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
    r.setVisualTime(38.f);
    r.setShaderQuality(gz::ShaderQuality::Quality);
    gz::Camera c;
    c.focusX=3004.f;c.focusZ=1270.f;c.distance=200.f;c.pitch=.60f;c.yaw=.48f;
    r.draw(sim,c,0);
    if(!r.savePPM("preview_v018_close.ppm"))return 1;
    std::cout<<"Close triangles: "<<r.lastTriangles()<<"\n";
    sim.setMode(gz::ViewMode::Direct);
    c.focusX=3000.f;c.focusZ=1240.f;c.distance=360.f;c.pitch=.58f;c.yaw=.43f;
    r.draw(sim,c,0);
    if(!r.savePPM("preview_v018_village.ppm"))return 2;
    std::cout<<"Village triangles: "<<r.lastTriangles()<<"\n";
    gz::AtmosphereSettings sunset;sunset.time=gz::TimeOfDay::Sunset;
    r.setAtmosphere(sunset);
    c.focusX=3012.f;c.focusZ=1268.f;c.distance=230.f;c.pitch=.61f;c.yaw=.49f;
    r.draw(sim,c,0);
    if(!r.savePPM("preview_v018_sunset.ppm"))return 3;
    std::cout<<"Sunset triangles: "<<r.lastTriangles()<<"\n";
    return 0;
}
