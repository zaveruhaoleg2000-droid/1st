#include "gz/Renderer.hpp"
#include "gz/Terrain.hpp"
#include "gz/Simulation.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

static double ms(const std::chrono::steady_clock::time_point& started){
    return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
}
static void present(const char* label,std::vector<double> samples){
    if(samples.empty())return;
    std::sort(samples.begin(),samples.end());
    const double p50=samples[samples.size()/2];
    const double p95=samples[std::min(samples.size()-1,(samples.size()*95+99)/100-1)];
    std::cout<<label<<" p50="<<p50<<" ms, p95="<<p95<<" ms ("<<samples.size()<<" frames)\n";
}
int main(int argc,char**argv){
    const int frames=argc>1?std::clamp(std::atoi(argv[1]),3,100):20;
    const int width=argc>2?std::clamp(std::atoi(argv[2]),320,1600):640;
    const int height=argc>3?std::clamp(std::atoi(argv[3]),240,1000):400;
    gz::Simulation sim(4);sim.newOperation(96);
    gz::Renderer renderer;renderer.resize(width,height);
    renderer.setTerrainDirectory("assets/terrain");renderer.loadTextures("assets/textures/runtime",256);
    renderer.setVisualTime(7.f);
    gz::Camera cam;cam.focusX=3000;cam.focusZ=1250;cam.distance=1650;cam.pitch=.85f;
    const auto cold=std::chrono::steady_clock::now();renderer.draw(sim,cam,0);
    std::cout<<"GRAY ZONE CPU benchmark "<<width<<"x"<<height<<": first="<<ms(cold)
             <<" ms, generated="<<renderer.terrainLoads()<<" sectors, cache="
             <<renderer.terrainCacheBytes()/1048576.0<<" MiB\n";
    std::vector<double> times;
    for(int i=0;i<frames;++i){auto start=std::chrono::steady_clock::now();renderer.draw(sim,cam,0);times.push_back(ms(start));}
    present("Tactical",times);
    sim.setMode(gz::ViewMode::GlobalAI);
    times.clear();for(int i=0;i<frames;++i){auto start=std::chrono::steady_clock::now();renderer.draw(sim,cam,0);times.push_back(ms(start));}
    present("Strategic",times);
    std::cout<<"Note: isolated CPU rendering only; window compositor and GPU not measured.\n";
    return 0;
}
