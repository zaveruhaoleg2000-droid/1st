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
    r.setVisualTime(32.f);
    r.setShaderQuality(gz::ShaderQuality::Quality);
    gz::Camera c;
    c.focusX=3010.f;c.focusZ=1275.f;c.distance=210.f;c.pitch=.60f;c.yaw=.45f;
    r.draw(sim,c,0);
    if(!r.savePPM("preview_v017_close.ppm"))return 1;
    std::cout<<"Close triangles: "<<r.lastTriangles()<<"\n";
    sim.setMode(gz::ViewMode::Direct);
    c.focusX=3040.f;c.focusZ=1245.f;c.distance=470.f;c.pitch=.59f;c.yaw=.35f;
    r.draw(sim,c,0);
    if(!r.savePPM("preview_v017_village.ppm"))return 2;
    std::cout<<"Village triangles: "<<r.lastTriangles()<<"\n";
    gz::AtmosphereSettings sunset;sunset.time=gz::TimeOfDay::Sunset;
    r.setAtmosphere(sunset);
    c.focusX=3025.f;c.focusZ=1262.f;c.distance=250.f;c.pitch=.62f;c.yaw=.42f;
    r.draw(sim,c,0);
    if(!r.savePPM("preview_v017_sunset.ppm"))return 3;
    std::cout<<"Sunset triangles: "<<r.lastTriangles()<<"\n";
    return 0;
}
