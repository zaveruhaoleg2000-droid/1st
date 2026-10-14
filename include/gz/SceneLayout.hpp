#pragma once
#include "gz/Renderer.hpp"
#include "gz/ModelLod.hpp"
#include <vector>
namespace gz {
inline float sceneHash(int x,int z){std::uint32_t a=std::uint32_t(x)*374761393u+std::uint32_t(z)*668265263u+0x9e3779b9u;a=(a^(a>>13))*1274126177u;return float((a^(a>>16))&65535u)/65535.f;}
struct TreeInstance {float x,y,z,height,width;std::uint32_t seed;ModelLod lod;};
inline std::vector<TreeInstance> treeLayout(TerrainWorld& world,const Camera& camera){
    const bool closeView=camera.distance<160;const float extent=closeView?285:std::clamp(camera.distance*1.05f,780.f,1900.f),spacing=closeView?25.f:76.f,scale=closeView?.78f:1.2f;
    std::vector<TreeInstance> trees;trees.reserve(420);
    for(int i=int(std::floor((camera.focusX-extent)/spacing));i<=int(std::ceil((camera.focusX+extent)/spacing))&&trees.size()<420;++i)
    for(int j=int(std::floor((camera.focusZ-extent)/spacing));j<=int(std::ceil((camera.focusZ+extent)/spacing))&&trees.size()<420;++j){
        if(sceneHash(i+13,j+7)<.74f)continue;
        float x=(i+.25f+sceneHash(i,j)*.5f)*spacing,z=(j+.2f+sceneHash(i+55,j)*.6f)*spacing;
        if(std::abs(z-1200.f-30.f*std::sin(x*.001f))<32||std::abs(x-3000)<28)continue;
        if(std::abs(x-TerrainGenerator::riverCenter(z,world.generatorSettings()))<TerrainGenerator::riverWidth(z,world.generatorSettings())+6)continue;
        int hi=int(std::round((x-3000)/42)),hj=int(std::round((z-1220)/46));
        if(hi>=-10&&hi<=10&&hj>=-6&&hj<=6&&sceneHash(hi+222,hj+400)>=.65f&&std::abs(1220.f+hj*46.f-1200)>=24&&std::abs(hi*42.f)>=25&&std::abs(x-(3000+hi*42))<13&&std::abs(z-(1220+hj*46))<14)continue;
        auto seed=std::uint32_t(sceneHash(i*3,j*17)*65535);float h=(9.f+seed%7)*scale;
        trees.push_back({x,world.heightAt(x,z),z,h,h*.58f,seed,modelLod(std::hypot(std::hypot(x-camera.focusX,z-camera.focusZ),camera.distance))});
    }return trees;
}
}
