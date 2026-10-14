#pragma once
#include "gz/CameraPicking.hpp"
#include "gz/NativeScene.hpp"
#include "gz/Campaign.hpp"
#include <vector>
namespace gz {
struct HudVertex {float x,y,u,v,r,g,b,a;};
static_assert(sizeof(HudVertex)==32);
enum class HudAction {None,Pause,Hold,Map,Deploy,Repeat,Select,Focus,Quality,FirstPerson,Tactical,CycleUnit,Reload};
struct HudCommand {HudAction action=HudAction::None;int argument=0;float worldX=0,worldZ=0;};
class TacticalHud {
    struct Button {float x,y,w,h;HudCommand command;};
    std::vector<HudVertex> vertices_;
    std::vector<std::uint32_t> indices_;
    std::vector<Button> buttons_;
    std::vector<std::uint8_t> glyphs_,advances_;
    int glyphWidth_=576,glyphHeight_=660,width_=0,height_=0;
    std::uint64_t revision_=0;
    void rect(float x,float y,float width,float height,std::uint32_t color,float alpha=.92f);
    void text(float x,float y,std::u32string_view value,float size=16,std::uint32_t color=0xd5e6de);
    void label(float x,float y,float w,float h,std::u32string_view name,HudCommand command,bool active=false);
public:
    bool loadFont(const std::string& path);
    void build(const Simulation& simulation,TerrainWorld& terrain,const Camera& camera,
               const CampaignState* campaign,int selected,int width,int height,bool paused,int quality=2,bool terrainLoading=false);
    GeometryView geometry()const{return {vertices_.data(),vertices_.size()*sizeof(HudVertex),sizeof(HudVertex),indices_.data(),indices_.size(),0,0,0,0,revision_};}
    void drawCpu(std::uint32_t* pixels,int width,int height)const;
    HudCommand hit(float x,float y)const;
    bool contains(float x,float y)const;
};
}
