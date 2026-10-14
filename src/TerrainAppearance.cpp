#include "gz/TerrainAppearance.hpp"
#include "gz/LandscapeVisual.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
namespace gz {
std::shared_ptr<TerrainMacroData> TerrainAppearance::generate(WorldSettings settings,float originX,float originZ,std::uint32_t resolution,std::uint64_t revision){
    if(!settings.valid()||resolution<128||resolution>4096||(resolution&(resolution-1)))return {};
    auto result=std::make_shared<TerrainMacroData>();result->settings=settings;result->resolution=resolution;
    result->originX=originX;result->originZ=originZ;result->revision=revision;
    std::vector<std::uint8_t> pixels(std::size_t(resolution)*resolution*4);
    const float spacing=result->extent/resolution;
    const float seedOffset=float(settings.seed&65535u)*.043f;
    constexpr std::uint32_t cells=256;
    std::vector<std::array<float,3>> coarse((cells+1)*(cells+1));
    for(std::uint32_t y=0;y<=cells;++y)for(std::uint32_t x=0;x<=cells;++x){
        const float wx=originX+x*result->extent/cells,wz=originZ+y*result->extent/cells;
        const float h=TerrainGenerator::height(wx,wz,settings);
        const float dx=TerrainGenerator::height(wx+8,wz,settings)-h,dz=TerrainGenerator::height(wx,wz+8,settings)-h;
        const auto surface=TerrainGenerator::surface(wx,wz,h,std::atan(std::hypot(dx,dz)/8),settings);
        const float field=landscapeNoise(wx+seedOffset,wz-seedOffset,170.f);
        const float large=landscapeNoise(wx-seedOffset,wz+seedOffset,670.f);
        const float grass=settings.region==TerrainRegion::Forest?.80f:settings.region==TerrainRegion::Wetland?.87f:1.f;
        std::array<float,3> color{(80.f+field*30.f+large*8.f)*grass,84.f+field*27.f+large*6.f,52.f+field*20.f};
        auto blend=[&](std::array<float,3> target,float amount){for(int c=0;c<3;++c)color[c]=color[c]*(1-amount)+target[c]*amount;};
        blend({117,94,59},surface.soil*.75f);
        blend({99,101,96},surface.rock);
        blend({61,69,48},surface.wetness*.7f);

        coarse[std::size_t(y)*(cells+1)+x]=color;
    }
    // Low-frequency terrain color is evaluated once on a bounded grid.
    // Roads and subtle world-anchored variation retain the full output resolution.
    std::vector<float> roadRows(resolution),roadCols(resolution);
    for(std::uint32_t i=0;i<resolution;++i){roadRows[i]=roadCenterVertical(originZ+(i+.5f)*spacing);roadCols[i]=roadCenterHorizontal(originX+(i+.5f)*spacing);}
    for(std::uint32_t y=0;y<resolution;++y)for(std::uint32_t x=0;x<resolution;++x){
        const float gx=(x+.5f)*cells/resolution,gz=(y+.5f)*cells/resolution;
        const auto ix=std::min(std::uint32_t(gx),cells-1),iz=std::min(std::uint32_t(gz),cells-1);
        const float tx=gx-ix,tz=gz-iz;
        const auto& a=coarse[std::size_t(iz)*(cells+1)+ix];const auto& b=coarse[std::size_t(iz)*(cells+1)+ix+1];
        const auto& c=coarse[std::size_t(iz+1)*(cells+1)+ix];const auto& d=coarse[std::size_t(iz+1)*(cells+1)+ix+1];
        const float wx=originX+(x+.5f)*spacing,wz=originZ+(y+.5f)*spacing;
        const float roadDistance=std::min(std::abs(wz-roadCols[x]),std::abs(wx-roadRows[y]));
        const float road=(1-std::clamp((roadDistance-10.f)/5.f,0.f,1.f))*.9f;
        const float variation=.985f+.03f*landscapeHash(int(std::floor(wx/2)),int(std::floor(wz/2)));
        const auto offset=(std::size_t(y)*resolution+x)*4;
        const std::array<float,3> roadColor{114,111,95};
        for(int component=0;component<3;++component){
            const float rgb=((a[component]*(1-tx)+b[component]*tx)*(1-tz)+(c[component]*(1-tx)+d[component]*tx)*tz)*variation;
            pixels[offset+component]=std::uint8_t(std::clamp(rgb*(1-road)+roadColor[component]*road,0.f,255.f));
        }pixels[offset+3]=255;
    }
    // Exact 2x2 mip averages in linear color, then encode sRGB again.
    std::array<float,256> decode{};for(int i=0;i<256;++i){float v=i/255.f;decode[i]=v<=.04045f?v/12.92f:std::pow((v+.055f)/1.055f,2.4f);}
    std::array<std::uint8_t,65536> encode{};
    for(int i=0;i<65536;++i){float v=i/65535.f;encode[i]=std::uint8_t(std::clamp((v<=.0031308f?v*12.92f:1.055f*std::pow(v,1.f/2.4f)-.055f)*255.f+.5f,0.f,255.f));}
    for(std::uint32_t side=resolution;;side/=2){
        result->pixels.insert(result->pixels.end(),pixels.begin(),pixels.end());if(side==1)break;
        const auto half=side/2;std::vector<std::uint8_t> next(std::size_t(half)*half*4);
        for(std::uint32_t y=0;y<half;++y)for(std::uint32_t x=0;x<half;++x){const auto a=(std::size_t(y*2)*side+x*2)*4;
            for(int c=0;c<3;++c){const float v=(decode[pixels[a+c]]+decode[pixels[a+4+c]]+decode[pixels[a+side*4+c]]+decode[pixels[a+side*4+4+c]])*.25f;
                next[(std::size_t(y)*half+x)*4+c]=encode[std::size_t(std::clamp(v*65535.f+.5f,0.f,65535.f))];}
            next[(std::size_t(y)*half+x)*4+3]=255;
        }pixels=std::move(next);
    }
    return result;
}
void TerrainAppearance::update(WorldSettings settings,float focusX,float focusZ,std::uint32_t resolution){
    if(!settings.valid())return;
    resolution=resolution>=4096?4096:resolution>=2048?2048:resolution>=1024?1024:256;
    wanted_=settings;wantedResolution_=resolution;
    wantedX_=std::floor((focusX-4096.f)/1024.f)*1024.f;wantedZ_=std::floor((focusZ-4096.f)/1024.f)*1024.f;
    if(pending_.valid()){
        if(pending_.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
        auto ready=pending_.get();
        if(ready&&ready->settings==wanted_&&ready->resolution==wantedResolution_&&
            std::abs(ready->originX-wantedX_)<=1024&&std::abs(ready->originZ-wantedZ_)<=1024)current_=std::move(ready);
    }
    if(current_&&current_->settings==settings&&current_->resolution==resolution&&
        focusX>current_->originX+1500&&focusX<current_->originX+current_->extent-1500&&
        focusZ>current_->originZ+1500&&focusZ<current_->originZ+current_->extent-1500)return;
    const auto originX=wantedX_,originZ=wantedZ_;const auto revision=++nextRevision_;
    pending_=std::async(std::launch::async,[settings,originX,originZ,resolution,revision]{return generate(settings,originX,originZ,resolution,revision);});
}
}
