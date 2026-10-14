#include "gz/Simulation.hpp"
#include "gz/Renderer.hpp"
#include <chrono>
#include <iostream>
#include <string>
#include <filesystem>
int main(int argc,char** argv){
    const std::string target=(argc>1)?argv[1]:"preview.ppm";
    gz::Simulation sim(4);
    sim.newOperation(96);
    sim.orderSquad(0,3130,1210);
    sim.step(20);
    gz::Renderer renderer;
    renderer.resize(1100,680);
    renderer.setTerrainDirectory("assets/terrain");
    renderer.loadTextures("assets/textures/runtime",256);
    gz::Camera camera;camera.focusX=3000;camera.focusZ=1250;camera.distance=1650;camera.pitch=.85f;
    auto start=std::chrono::steady_clock::now();
    renderer.setVisualTime(5.3f);
    renderer.draw(sim,camera,0);
    const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    bool saved=renderer.savePPM(target);
    std::cout<<"GRAY ZONE preview: "<<target<<" | saved="<<saved<<" | "<<renderer.lastTriangles()<<" triangles | "<<ms<<" ms (this machine)\n";
    // Screenshot of direct mode (following friendly squad #0).
    const auto& soldier=sim.soldier(0);
    sim.setMode(gz::ViewMode::Direct);camera.focusX=float(soldier.x);camera.focusZ=float(soldier.z);camera.distance=28;camera.pitch=.46f;
    renderer.draw(sim,camera,0);
    auto direct=std::filesystem::path(target);direct.replace_filename(direct.stem().string()+"_direct.ppm");
    bool saved2=renderer.savePPM(direct.string());
    sim.setMode(gz::ViewMode::GlobalAI);renderer.draw(sim,camera,0);
    auto global=std::filesystem::path(target);global.replace_filename(global.stem().string()+"_global.ppm");
    bool saved3=renderer.savePPM(global.string());
    // Water showcase for visually checking animated ripples and reflective bank colors.
    sim.setMode(gz::ViewMode::Command);
    camera.focusX=3380.f;camera.focusZ=1420.f;camera.distance=510.f;camera.pitch=.47f;camera.yaw=.26f;
    renderer.setVisualTime(12.5f);
    renderer.draw(sim,camera,0);
    auto water=std::filesystem::path(target);water.replace_filename(water.stem().string()+"_water.ppm");
    bool saved4=renderer.savePPM(water.string());
    // Inspect the hand-painted 16-bit heightmap near its added hill.
    camera.focusX=3620.f;camera.focusZ=1700.f;camera.distance=520.f;camera.pitch=.65f;
    camera.yaw=.28f;
    renderer.setVisualTime(15.f);
    const auto hillStart=std::chrono::steady_clock::now();
    renderer.draw(sim,camera,0);
    const auto hillMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-hillStart).count();
    auto hill=std::filesystem::path(target);hill.replace_filename(hill.stem().string()+"_terrain.ppm");
    bool saved5=renderer.savePPM(hill.string());
    std::cout<<"Painted heightmap close view: "<<hill.string()<<" | "<<renderer.lastTriangles()<<" triangles | "<<hillMs<<" ms (cache warm) | "<<renderer.terrainCacheBytes()/1024/1024<<" MiB terrain cache\n";
    return saved && saved2 && saved3 && saved4 && saved5?0:1;
}
