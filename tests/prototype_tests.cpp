#include "gz/Simulation.hpp"
#include "gz/Renderer.hpp"
#include <iostream>
#include <filesystem>
#include <cmath>
int main(){
    gz::Simulation sim(4);sim.newOperation(800);
    if(sim.soldierCount()!=1600 || sim.laneCount()!=4)return 1;
    if(!sim.selectControlledSquad(1))return 2;
    const auto soldierIndex=sim.controlledSoldier();
    if(soldierIndex!=8)return 3;
    const auto before=sim.soldier(soldierIndex).x;
    sim.setMode(gz::ViewMode::Direct);
    if(!sim.moveCommander(before+60.,sim.soldier(soldierIndex).z))return 4;
    sim.step(20);
    if(sim.soldier(soldierIndex).x<=before)return 5;
    auto s=sim.soldier(soldierIndex);s.x=2999.9;
    if(sim.sectorOf(s).x!=0)return 6;
    s.x=3000.1;if(sim.sectorOf(s).x!=1)return 7;
    // Global mode gives control to AI without resetting identities or losses.
    const auto x=sim.soldier(soldierIndex).x;
    sim.setMode(gz::ViewMode::GlobalAI);sim.step(20);
    if(sim.soldierCount()!=1600 || sim.soldier(soldierIndex).id!=soldierIndex || std::abs(sim.soldier(soldierIndex).x-x)>10)return 8;
    gz::Renderer renderer;renderer.resize(640,400);
    gz::Camera camera;camera.focusX=3000;camera.focusZ=1250;camera.distance=1600;
    sim.setMode(gz::ViewMode::Command);
    renderer.draw(sim,camera,1);
    auto sample=renderer.data()[200*renderer.width()+320];
    if(sample==0 || renderer.lastTriangles()<500)return 9;
    int sx,sy;
    if(!renderer.project(camera,camera.focusX,gz::Renderer::terrainHeight(camera.focusX,camera.focusZ),camera.focusZ,sx,sy))return 10;
    float xg=0,zg=0;
    if(!renderer.screenToGround(camera,sx,sy,xg,zg) || std::abs(xg-camera.focusX)>25 || std::abs(zg-camera.focusZ)>25)return 11;
    std::cout<<"PROTOTYPE TESTS PASSED (1600 units, 4 lanes, selection, handoff, terrain, projection)\n";
    return 0;
}
