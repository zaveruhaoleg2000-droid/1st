#include "gz/Renderer.hpp"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>

static std::uint64_t changed(const std::vector<std::uint32_t>& a,const std::vector<std::uint32_t>& b){
    std::uint64_t count=0;
    for(std::size_t i=0;i<a.size();++i)count+=(a[i]!=b[i]);
    return count;
}
int main(){
    gz::Simulation sim(4);sim.newOperation(96);
    gz::Renderer r;r.resize(600,360);
    gz::Camera c;c.focusX=3380;c.focusZ=1420;c.distance=510;c.pitch=.47f;c.yaw=.26f;
    auto render=[&](float seconds,gz::VisualEffects fx){
        r.setEffects(fx);r.setVisualTime(seconds);r.draw(sim,c,0);
        return std::vector<std::uint32_t>(r.data(),r.data()+r.width()*r.height());
    };
    gz::VisualEffects fx{};
    auto baseline=render(12.5f,fx);
    fx.antialiasing=false;
    auto noAA=render(12.5f,fx);
    if(changed(baseline,noAA)<100){std::cerr<<"AA did not change enough edge pixels\n";return 1;}
    fx.antialiasing=true;fx.wind=false;
    auto noWind=render(12.5f,fx);
    if(changed(baseline,noWind)<10){std::cerr<<"Canopy wind not visible\n";return 2;}
    fx.wind=true;fx.waterReflections=false;
    auto noReflection=render(12.5f,fx);
    if(changed(baseline,noReflection)<250){std::cerr<<"Water reflection not visible\n";return 3;}
    fx.waterReflections=true;
    auto animated=render(15.5f,fx);
    if(changed(baseline,animated)<300){std::cerr<<"Water/wind not animated\n";return 4;}
    auto sameTime=render(12.5f,fx);
    if(changed(baseline,sameTime)!=0){std::cerr<<"Render not deterministic for identical visual time\n";return 5;}
    std::cout<<"VISUAL FX PASS: AA "<<changed(baseline,noAA)<<", wind "<<changed(baseline,noWind)
             <<", reflections "<<changed(baseline,noReflection)<<", animation "<<changed(baseline,animated)<<" changed pixels\n";
    return 0;
}
