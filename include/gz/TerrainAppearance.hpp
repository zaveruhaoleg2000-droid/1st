#pragma once
#include "gz/TerrainGenerator.hpp"
#include <future>
#include <memory>
#include <vector>
namespace gz {
struct TerrainMacroView {
    const std::uint8_t* pixels=nullptr;
    std::size_t bytes=0;
    std::uint32_t resolution=0;
    float originX=0,originZ=0,extent=8192;
    std::uint64_t revision=0;
    bool valid()const{return pixels&&bytes&&resolution>=128&&resolution<=4096&&(resolution&(resolution-1))==0&&extent>0;}
};
struct TerrainMacroData {
    std::vector<std::uint8_t> pixels;
    WorldSettings settings{};
    std::uint32_t resolution=0;
    float originX=0,originZ=0,extent=8192;
    std::uint64_t revision=0;
    TerrainMacroView view()const{return {pixels.data(),pixels.size(),resolution,originX,originZ,extent,revision};}
};
class TerrainAppearance {
    std::future<std::shared_ptr<TerrainMacroData>> pending_;
    std::shared_ptr<TerrainMacroData> current_;
    WorldSettings wanted_{};
    std::uint32_t wantedResolution_=0;
    float wantedX_=0,wantedZ_=0;
    std::uint64_t nextRevision_=0;
public:
    void update(WorldSettings settings,float focusX,float focusZ,std::uint32_t resolution=2048);
    bool loading()const{return pending_.valid();}
    TerrainMacroView view()const{return current_&&current_->settings==wanted_&&current_->resolution==wantedResolution_?current_->view():TerrainMacroView{};}
    static std::shared_ptr<TerrainMacroData> generate(WorldSettings settings,float originX,float originZ,std::uint32_t resolution,std::uint64_t revision=1);
};
}
