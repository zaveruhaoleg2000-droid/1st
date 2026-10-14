#include "gz/Europe.hpp"
#include "gz/Diagnostics.hpp"
#include "gz/Renderer.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>
namespace {
bool writePPM(const char* path,int w,int h,const std::uint32_t* pixels){
    std::ofstream out(path,std::ios::binary);
    if(!out)return false;
    out<<"P6\n"<<w<<' '<<h<<"\n255\n";
    for(std::size_t i=0;i<std::size_t(w)*h;++i){
        const std::uint32_t c=pixels[i];
        const char rgb[]={char(c>>16),char(c>>8),char(c)};
        out.write(rgb,3);
    }
    return bool(out);
}
}
int main(){
    gz::EuropeBackdrop map;
    if(!map.load("assets/europe/europe_vertices.gzm")||!map.loadPolitics("assets/politics/political_1989.gzp"))return 2;
    map.setReliefScale(.006f);
    std::vector<std::uint32_t> global;
    map.render(960,600,global,0,true,true);
    // Warsaw is used only as a click location; the game selects the nearest real vertex.
    const float u=(21.f+20.f)/80.f,v=(76.f-52.f)/49.f;
    const int x=int(960.f*(.015f+u*.955f+(v-.5f)*.032f));
    const int y=int(600.f*(.048f+v*.906f+u*.032f));
    if(!map.selectAt(x,y,19.f))return 3;
    map.render(960,600,global,0,true,true);
    gz::DebugHud hud;if(!hud.load("assets/ui/glyphs.pgm"))return 4;
    gz::FrameMeter meter;
    for(int i=0;i<60;++i)meter.record(7.5f,8.7f);
    auto stats=meter.snapshot(map.triangles(),0,0,"Software");
    hud.draw(global.data(),960,600,stats);
    if(!writePPM("preview_v014_strategy.ppm",960,600,global.data()))return 5;
    gz::Simulation sim(4);sim.newOperation(96);sim.setMode(gz::ViewMode::Command);
    gz::Renderer renderer;renderer.resize(960,600);
    renderer.setTerrainDirectory("assets/terrain");
    renderer.loadTextures("assets/textures/runtime",128);
    renderer.setVisualTime(2.f);
    gz::Camera camera{};
    renderer.draw(sim,camera,0);
    for(int i=0;i<60;++i)meter.record(26.2f,28.0f);
    auto metrics=meter.snapshot(renderer.lastTriangles(),renderer.terrainCacheBytes(),0,"Software");
    hud.draw(renderer.mutablePixels(),renderer.width(),renderer.height(),metrics);
    if(!writePPM("preview_v014_tactical.ppm",renderer.width(),renderer.height(),renderer.data()))return 6;
    std::cout<<"Preview created: map selection at lon "<<map.selection()->longitude
             <<" lat "<<map.selection()->latitude<<" tactical triangles "<<renderer.lastTriangles()<<'\n';
}
