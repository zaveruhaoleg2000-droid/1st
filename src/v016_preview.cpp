#include "gz/Renderer.hpp"
#include <filesystem>
#include <iostream>
int main(){
    gz::Simulation sim(4);
    sim.newOperation(96);
    sim.setMode(gz::ViewMode::Command);
    gz::Renderer r;
    r.resize(960,600);
    r.setTerrainDirectory("assets/terrain");
    r.loadTextures("assets/textures/runtime",128);
    r.setVisualTime(26.f);
    r.setShaderQuality(gz::ShaderQuality::Quality);
    gz::Camera c;
    c.focusX=3000.f;c.focusZ=1260.f;c.distance=440.f;c.pitch=.59f;c.yaw=.35f;
    r.draw(sim,c,0);
    if(!r.savePPM("preview_v016_village.ppm"))return 1;
    std::cout<<"Village triangles: "<<r.lastTriangles()<<"\n";
    c.focusX=3080.f;c.focusZ=1290.f;c.distance=175.f;c.pitch=.60f;
    r.draw(sim,c,0);
    if(!r.savePPM("preview_v016_close.ppm"))return 2;
    std::cout<<"Close triangles: "<<r.lastTriangles()<<"\n";
    gz::AtmosphereSettings sunset;sunset.time=gz::TimeOfDay::Sunset;
    r.setAtmosphere(sunset);r.draw(sim,c,0);
    if(!r.savePPM("preview_v016_sunset.ppm"))return 3;
    std::cout<<"Sunset triangles: "<<r.lastTriangles()<<"\n";
    return 0;
}
