#include "gz/RenderGraph.hpp"
#include <algorithm>
#include <array>
#include <limits>
namespace gz {
const char* renderStageName(RenderStage stage)noexcept{
    switch(stage){
    case RenderStage::Upload:return "upload";
    case RenderStage::Depth:return "depth";
    case RenderStage::Terrain:return "terrain";
    case RenderStage::StaticGeometry:return "static_geometry";
    case RenderStage::Units:return "units";
    case RenderStage::Vegetation:return "vegetation";
    case RenderStage::Water:return "water";
    case RenderStage::DynamicLighting:return "lighting";
    case RenderStage::WeatherFX:return "weather";
    case RenderStage::Interface:return "interface";
    case RenderStage::Present:return "present";
    }
    return "unknown";
}
RenderGraphPlan RenderGraphPlan::cpuFrameUpload(){
    RenderGraphPlan graph;
    graph.passes_={{RenderStage::Upload,{}},{RenderStage::Present,{RenderStage::Upload}}};
    return graph;
}
RenderGraphPlan RenderGraphPlan::nativeScene(){
    RenderGraphPlan graph;
    graph.passes_={
        {RenderStage::Upload,{}},
        {RenderStage::Depth,{RenderStage::Upload}},
        {RenderStage::Terrain,{RenderStage::Depth}},
        {RenderStage::StaticGeometry,{RenderStage::Terrain}},
        {RenderStage::Units,{RenderStage::StaticGeometry}},
        {RenderStage::Vegetation,{RenderStage::Depth}},
        {RenderStage::Water,{RenderStage::Terrain}},
        {RenderStage::DynamicLighting,{RenderStage::Units,RenderStage::Vegetation,RenderStage::Water}},
        {RenderStage::WeatherFX,{RenderStage::DynamicLighting}},
        {RenderStage::Interface,{RenderStage::WeatherFX}},
        {RenderStage::Present,{RenderStage::Interface}}
    };
    return graph;
}
bool RenderGraphPlan::validate(std::string& error)const{
    error.clear();
    if(passes_.empty()||passes_.size()>32){error="Invalid number of passes";return false;}
    std::array<bool,11> seen{};
    for(std::size_t i=0;i<passes_.size();++i){
        const auto& p=passes_[i];
        auto id=std::size_t(p.stage);
        if(id>=seen.size()||seen[id]){error="Duplicate or unknown render pass";return false;}
        if(p.stage==RenderStage::Present && i+1!=passes_.size()){
            error="Presentation must be the final pass";return false;
        }
        for(auto dependency:p.mustFollow){
            const auto depends=std::size_t(dependency);
            if(depends>=seen.size()||!seen[depends]){
                error="Pass dependency not satisfied for "+std::string(renderStageName(p.stage));
                return false;
            }
        }
        seen[id]=true;
    }
    if(!seen[std::size_t(RenderStage::Present)]){error="Missing present pass";return false;}
    return true;
}
bool GpuBudgetLedger::acquire(std::uint64_t id,std::size_t bytes){
    const auto old=std::find_if(resources_.begin(),resources_.end(),[&](auto& p){return p.first==id;});
    const auto previous=old!=resources_.end()?old->second:0;
    const auto base=used_-previous;
    if(bytes>limit_-base)return false;
    if(old!=resources_.end())old->second=bytes;
    else resources_.emplace_back(id,bytes);
    used_=base+bytes;
    return true;
}
bool GpuBudgetLedger::release(std::uint64_t id){
    const auto it=std::find_if(resources_.begin(),resources_.end(),[&](auto& p){return p.first==id;});
    if(it==resources_.end())return false;
    used_-=it->second;resources_.erase(it);return true;
}
}
