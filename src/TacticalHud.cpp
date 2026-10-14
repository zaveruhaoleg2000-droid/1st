#include "gz/TacticalHud.hpp"
#include "gz/Glyphs.hpp"
#include <algorithm>
#include <fstream>
#include <string>
namespace gz {
namespace {
std::u32string utf8(std::string_view input){
    std::u32string out;
    for(std::size_t i=0;i<input.size();){unsigned char c=input[i++];char32_t code=c;int continuation=0;
        if(c>=0xf0){code=c&7;continuation=3;}else if(c>=0xe0){code=c&15;continuation=2;}else if(c>=0xc0){code=c&31;continuation=1;}
        if(i+continuation>input.size())break;
        for(int j=0;j<continuation;++j)code=(code<<6)|(static_cast<unsigned char>(input[i++])&63);
        out.push_back(code);
    }return out;
}
std::u32string number(std::uint64_t n){return utf8(std::to_string(n));}
}
bool TacticalHud::loadFont(const std::string& path){
    std::ifstream in(path,std::ios::binary);std::string magic;int maximum=0;
    if(!(in>>magic>>glyphWidth_>>glyphHeight_>>maximum)||magic!="P5"||maximum!=255)return false;
    return glyphWidth_>=kGlyphCols*kGlyphCellW&&glyphHeight_>=kGlyphCellH;
}
void TacticalHud::rect(float x,float y,float w,float h,std::uint32_t rgb,float alpha){
    const float r=((rgb>>16)&255)/255.f,g=((rgb>>8)&255)/255.f,b=(rgb&255)/255.f;
    const auto base=std::uint32_t(vertices_.size());
    for(const auto p:std::array<std::array<float,2>,4>{{{x,y},{x+w,y},{x+w,y+h},{x,y+h}}})vertices_.push_back({p[0],p[1],-1,-1,r,g,b,alpha});
    indices_.insert(indices_.end(),{base,base+1,base+2,base,base+2,base+3});
}
void TacticalHud::text(float x,float y,std::u32string_view value,float size,std::uint32_t rgb){
    const float advance=size*.68f,r=((rgb>>16)&255)/255.f,g=((rgb>>8)&255)/255.f,b=(rgb&255)/255.f;
    for(auto ch:value){if(x+advance>width_-12)break;
        std::size_t glyph=0;while(kMenuGlyphs[glyph]&&kMenuGlyphs[glyph]!=ch)++glyph;
        if(!kMenuGlyphs[glyph])glyph=std::u32string_view(kMenuGlyphs).find(U'?');
        const float u=float(glyph%kGlyphCols*kGlyphCellW)/glyphWidth_,v=float(glyph/kGlyphCols*kGlyphCellH)/glyphHeight_;
        const float du=float(kGlyphCellW)/glyphWidth_,dv=float(kGlyphCellH)/glyphHeight_;
        const auto base=std::uint32_t(vertices_.size());
        vertices_.insert(vertices_.end(),{{x,y,u,v,r,g,b,1},{x+advance,y,u+du,v,r,g,b,1},{x+advance,y+size,u+du,v+dv,r,g,b,1},{x,y+size,u,v+dv,r,g,b,1}});
        indices_.insert(indices_.end(),{base,base+1,base+2,base,base+2,base+3});x+=advance;
    }
}
void TacticalHud::label(float x,float y,float w,float h,std::u32string_view name,HudCommand command,bool active){
    rect(x,y,w,h,active?0x37594a:0x253936);rect(x,y,w,2,active?0xc9ac67:0x54645a);
    text(x+8,y+(h-16)*.5f,name,16,active?0xe6ce95:0xd5e6de);buttons_.push_back({x,y,w,h,command});
}
void TacticalHud::build(const Simulation& sim,TerrainWorld& terrain,const Camera& camera,const CampaignState* campaign,int selected,int width,int height,bool paused,bool turns,int quality,bool terrainLoading){
    width_=width;height_=height;vertices_.clear();indices_.clear();buttons_.clear();++revision_;
    if(width<320||height<200)return;
    rect(10,10,float(width-20),74,0x111e20);rect(10,10,3,74,0xc8aa69);
    text(24,18,U"GRAY ZONE: 1989",21,0xe0c184);
    if(width>=960){text(float(width-360),20,terrain.generatorSettings().version?U"SEED "+number(terrain.generatorSettings().seed):U"КЛАСИЧНИЙ РЕЛЬЄФ",14,0xaec3ad);
        text(float(width-360),43,terrainLoading?U"ПОКРИТТЯ: ГЕНЕРАЦІЯ":U"ПОКРИТТЯ: ГОТОВЕ",13,0xbda971);}
    text(24,46,(paused?U"ПАУЗА":U"ОПЕРАЦІЯ")+std::u32string(U" · ")+(turns?U"ПОКРОКОВО":U"РЕАЛЬНИЙ ЧАС")+U" · ТИК "+number(sim.ticks()),15);
    if(campaign){
        const auto& c=campaign->contract;
        std::u32string objective=c.stage==ContractStage::Engaged?U"ЦІЛЬ: УТРИМАТИ ПЕРЕПРАВУ "+number(c.holdTicks)+U"/120":
            c.stage==ContractStage::Victory?U"ПЕРЕМОГА · НАГОРОДА "+number(c.lastReward):c.stage==ContractStage::Defeat?U"КОНТРАКТ ЗІРВАНО":U"ОБЕРІТЬ ЗАГІН І РОЗПОЧНІТЬ ОПЕРАЦІЮ";
        text(24,65,objective,14,0xd7d2ae);
    }
    if(width<800)return;
    const float bottom=float(height-122);
    rect(10,bottom,float(width-20),112,0x111e20);
    label(22,bottom+12,100,31,paused?U"ПРОДОВЖ":U"ПАУЗА",{HudAction::Pause},paused);
    label(130,bottom+12,115,31,turns?U"РЕАЛЬНИЙ":U"ПОКРОКОВО",{HudAction::ToggleTurns});
    label(253,bottom+12,75,31,U"ХІД",{HudAction::NextTurn});
    label(336,bottom+12,107,31,U"УТРИМАТИ",{HudAction::Hold});
    label(451,bottom+12,86,31,U"МАПА",{HudAction::Map});
    label(float(width-145),bottom+12,123,31,quality==2?U"ЯКІСТЬ: В":quality==1?U"ЯКІСТЬ: С":U"ЯКІСТЬ: Н",{HudAction::Quality});
    if(campaign&&campaign->contract.stage==ContractStage::Planning)label(550,bottom+12,122,31,U"ВИСАДКА",{HudAction::Deploy},true);
    if(campaign&&(campaign->contract.stage==ContractStage::Victory||campaign->contract.stage==ContractStage::Defeat))label(550,bottom+12,130,31,U"НОВИЙ БІЙ",{HudAction::Repeat},true);
    int cards=0;std::vector<std::size_t> friendly;
    for(std::size_t i=0;i<sim.squadCount();++i)if(sim.squad(i).side==Side::Player)friendly.push_back(i);
    const auto chosen=std::find(friendly.begin(),friendly.end(),std::size_t(std::max(selected,0)));
    const std::size_t first=chosen==friendly.end()?0:std::size_t(chosen-friendly.begin())/6*6;
    std::size_t ordinal=0;
    for(std::size_t i=0;i<sim.squadCount()&&cards<6;++i){const auto& squad=sim.squad(i);if(squad.side!=Side::Player)continue;
        if(ordinal++<first)continue;
        unsigned alive=0,ammo=0,health=0;for(std::size_t j=squad.memberStart;j<squad.memberStart+squad.memberCount;++j){if(sim.soldier(j).health>0){++alive;ammo+=sim.soldier(j).ammo;health+=unsigned(sim.soldier(j).health);}}
        const float x=22+cards*112.f;
        if(x+108>width-160)break;
        label(x,bottom+53,104,43,U"ЗАГІН "+number(i+1),{HudAction::Select,int(i)},int(i)==selected);
        text(x+8,bottom+81,number(alive)+U" / "+number(ammo),11);
        rect(x+8,bottom+99,88,3,0x35433a);rect(x+8,bottom+99,88*float(health)/std::max(1u,squad.memberCount*100u),3,0x87b88b,1);++cards;
    }
    text(float(width-140),bottom+69,U"ПКМ — НАКАЗ",13);
    text(float(width-140),bottom+87,U"TAB — ЗАГІН",13);
    if(selected>=0&&std::size_t(selected)<sim.squadCount()){
        const auto& squad=sim.squad(std::size_t(selected));
        const auto frame=makeGpuFrameConstants(makeGpuSceneCamera(camera,terrain.heightAt(camera.focusX,camera.focusZ),{}),width,height,0);
        for(std::size_t i=squad.memberStart;i<squad.memberStart+squad.memberCount;++i){const auto& soldier=sim.soldier(i);if(soldier.health<=0)continue;
            float x=0,y=0;if(projectGpuPoint(frame,width,height,float(soldier.x),terrain.heightAt(soldier.x,soldier.z)+2.1f,float(soldier.z),x,y)&&x>10&&x<width-10&&y>90&&y<bottom){rect(x-4,y-4,8,3,0xe7c477);}
        }
        if(squad.hasObjective){float x=0,y=0;if(projectGpuPoint(frame,width,height,float(squad.objectiveX),terrain.heightAt(squad.objectiveX,squad.objectiveZ)+.8f,float(squad.objectiveZ),x,y)&&y>90&&y<bottom){rect(x-8,y-1,16,2,0xe7c477);rect(x-1,y-8,2,16,0xe7c477);}}
    }
    const float mapSize=std::clamp(float(width)*.16f,120.f,190.f),mx=width-mapSize-22,my=98;
    if(my+mapSize<bottom-10){
        rect(mx-5,my-5,mapSize+10,mapSize+31,0x152421);text(mx,my+mapSize+5,U"ТАКТИЧНА МАПА",12,0xb9c5ad);
        const float range=1000.f;
        for(int z=0;z<32;++z)for(int x=0;x<32;++x){const float wx=camera.focusX+(x/31.f-.5f)*range,wz=camera.focusZ+(z/31.f-.5f)*range;
            const float h=terrain.heightAt(wx,wz);const auto green=std::uint32_t(std::clamp(45.f+h*.3f,30.f,90.f));
            const auto mask=TerrainGenerator::surface(wx,wz,h,0,terrain.generatorSettings());
            std::uint32_t color=(45u<<16)|((green+25u)<<8)|40u;
            if(mask.wetness>.4f)color=0x556448;if(mask.road>.3f)color=0x969382;if(mask.water>.1f)color=0x527a8d;
            rect(mx+x*mapSize/32,my+z*mapSize/32,mapSize/32+1,mapSize/32+1,color,1);
        }
        for(std::size_t i=0;i<sim.soldierCount();++i){const auto& soldier=sim.soldier(i);if(soldier.health<=0||sim.squad(soldier.squad).side!=Side::Player)continue;
            const float x=(float(soldier.x)-camera.focusX)/range+.5f,z=(float(soldier.z)-camera.focusZ)/range+.5f;
            if(x>=0&&x<=1&&z>=0&&z<=1)rect(mx+x*mapSize-1,my+z*mapSize-1,3,3,0xa1d9b4,1);
        }
        const float objectiveX=(3000.f-camera.focusX)/range+.5f,objectiveZ=(1150.f-camera.focusZ)/range+.5f;
        if(objectiveX>=0&&objectiveX<=1&&objectiveZ>=0&&objectiveZ<=1){rect(mx+objectiveX*mapSize-4,my+objectiveZ*mapSize-4,8,8,0xe6c787,1);}
        rect(mx+mapSize*.5f-2,my+mapSize*.5f-2,4,4,0xdfe6dc,1);
        buttons_.push_back({mx,my,mapSize,mapSize,{HudAction::Focus,0,camera.focusX,camera.focusZ}});
    }
    if(campaign&&!campaign->latestNews.empty())text(22,bottom-25,utf8(campaign->latestNews).substr(0,std::size_t(std::max(1,(width-50)/10))),14,0xe2c58d);
}
HudCommand TacticalHud::hit(float x,float y)const{
    for(auto it=buttons_.rbegin();it!=buttons_.rend();++it)if(x>=it->x&&x<=it->x+it->w&&y>=it->y&&y<=it->y+it->h){auto command=it->command;
        if(command.action==HudAction::Focus){command.worldX+=(x-it->x)/it->w*1000.f-500.f;command.worldZ+=(y-it->y)/it->h*1000.f-500.f;}return command;}
    return {};
}
bool TacticalHud::contains(float x,float y)const{return (y>=10&&y<=84)||(y>=height_-122&&y<height_)||hit(x,y).action!=HudAction::None;}
}
