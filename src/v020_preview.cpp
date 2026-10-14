#include "gz/Renderer.hpp"
#include <iostream>
int main(){
    gz::Simulation sim(4);
    sim.newOperation(48);
    sim.setMode(gz::ViewMode::Command);
    gz::Renderer r;
    r.resize(960,600);
    r.setTerrainDirectory("assets/terrain");
    if(r.loadTextures("assets/textures/runtime",128)<10)return 1;
    r.setVisualTime(41.f);
    r.setShaderQuality(gz::ShaderQuality::Quality);
    r.setPhotoMode(true);
    gz::Camera c;
    c.focusX=3028.f;c.focusZ=1209.f;c.distance=115.f;c.pitch=.57f;c.yaw=.57f;
    r.setBattlefieldDetail(false);
    r.draw(sim,c,-1);
    if(!r.savePPM("preview_v020_before.ppm"))return 2;
    std::cout<<"Before triangles="<<r.lastTriangles()<<"\n";
    r.setBattlefieldDetail(true);
    r.draw(sim,c,-1);
    if(!r.savePPM("preview_v020_checkpoint.ppm"))return 3;
    std::cout<<"Checkpoint triangles="<<r.lastTriangles()<<"\n";
    c.focusX=3030.f;c.focusZ=1199.f;c.distance=210.f;c.pitch=.58f;c.yaw=.46f;
    r.draw(sim,c,-1);
    if(!r.savePPM("preview_v020_battlefield.ppm"))return 4;
    std::cout<<"Battlefield triangles="<<r.lastTriangles()<<"\n";
    c.focusX=3041.f;c.focusZ=1219.f;c.distance=67.f;c.pitch=.44f;c.yaw=-.35f;
    r.draw(sim,c,-1);
    if(!r.savePPM("preview_v020_guardpost.ppm"))return 6;
    std::cout<<"Guardpost triangles="<<r.lastTriangles()<<"\n";
    c.focusX=3070.f;c.focusZ=1165.f;c.distance=94.f;c.pitch=.43f;c.yaw=.38f;
    r.draw(sim,c,-1);
    if(!r.savePPM("preview_v020_wreck.ppm"))return 7;
    std::cout<<"Wreck triangles="<<r.lastTriangles()<<"\n";
    gz::AtmosphereSettings sunset;sunset.time=gz::TimeOfDay::Sunset;
    r.setAtmosphere(sunset);
    c.focusX=3050.f;c.focusZ=1180.f;c.distance=138.f;c.pitch=.60f;c.yaw=.43f;
    r.draw(sim,c,-1);
    if(!r.savePPM("preview_v020_sunset.ppm"))return 5;
    std::cout<<"Sunset triangles="<<r.lastTriangles()<<"\n";
    return 0;
}
