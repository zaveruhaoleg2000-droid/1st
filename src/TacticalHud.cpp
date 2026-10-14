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
    if(glyphWidth_<kGlyphCols*kGlyphCellW||glyphHeight_<kGlyphCellH||glyphWidth_>2048||glyphHeight_>2048)return false;
    in.get();glyphs_.resize(std::size_t(glyphWidth_)*glyphHeight_);in.read(reinterpret_cast<char*>(glyphs_.data()),std::streamsize(glyphs_.size()));
    std::ifstream adv("assets/ui/advances.bin",std::ios::binary);advances_.assign(std::istreambuf_iterator<char>(adv),{});
    return bool(in)&&advances_.size()==std::u32string_view(kMenuGlyphs).size();
}
void TacticalHud::rect(float x,float y,float w,float h,std::uint32_t rgb,float alpha){
    const float r=((rgb>>16)&255)/255.f,g=((rgb>>8)&255)/255.f,b=(rgb&255)/255.f;
    const auto base=std::uint32_t(vertices_.size());
    for(const auto p:std::array<std::array<float,2>,4>{{{x,y},{x+w,y},{x+w,y+h},{x,y+h}}})vertices_.push_back({p[0],p[1],-1,-1,r,g,b,alpha});
    indices_.insert(indices_.end(),{base,base+1,base+2,base,base+2,base+3});
}
void TacticalHud::text(float x,float y,std::u32string_view value,float size,std::uint32_t rgb){
    const float scale=size/kGlyphCellH,r=((rgb>>16)&255)/255.f,g=((rgb>>8)&255)/255.f,b=(rgb&255)/255.f;
    for(auto ch:value){
        std::size_t glyph=0;while(kMenuGlyphs[glyph]&&kMenuGlyphs[glyph]!=ch)++glyph;
        if(!kMenuGlyphs[glyph])glyph=std::u32string_view(kMenuGlyphs).find(U'?');
        const float advance=(glyph<advances_.size()?advances_[glyph]:24)*scale;
        if(x+advance>width_-12)break;
        const float glyphDrawWidth=advance;
        const float u=(float(glyph%kGlyphCols*kGlyphCellW)+.5f)/glyphWidth_,v=(float(glyph/kGlyphCols*kGlyphCellH)+.5f)/glyphHeight_;
        const float du=(advance/scale-1.f)/glyphWidth_,dv=float(kGlyphCellH-1)/glyphHeight_;
        const auto base=std::uint32_t(vertices_.size());
        vertices_.insert(vertices_.end(),{{x,y,u,v,r,g,b,1},{x+glyphDrawWidth,y,u+du,v,r,g,b,1},{x+glyphDrawWidth,y+size,u+du,v+dv,r,g,b,1},{x,y+size,u,v+dv,r,g,b,1}});
        indices_.insert(indices_.end(),{base,base+1,base+2,base,base+2,base+3});x+=advance;
    }
}
void TacticalHud::label(float x,float y,float w,float h,std::u32string_view name,HudCommand command,bool active){
    rect(x,y,w,h,active?0x37594a:0x253936);rect(x,y,w,2,active?0xc9ac67:0x54645a);
    text(x+8,y+(h-16)*.5f,name,16,active?0xe6ce95:0xd5e6de);buttons_.push_back({x,y,w,h,command});
}
void TacticalHud::build(const Simulation& sim,TerrainWorld& terrain,const Camera& camera,const CampaignState* campaign,int selected,int width,int height,bool paused,int quality,bool terrainLoading){
    (void)terrainLoading;width_=width;height_=height;vertices_.clear();indices_.clear();buttons_.clear();++revision_;
    if(width<320||height<200)return;
    const auto stats=sim.stats();
    if(camera.firstPerson){
        const auto& soldier=sim.soldier(sim.controlledSoldier());const auto& weapon=weaponSpec(soldier.weapon);
        rect(width*.5f-155,14,310,38,0x101820,.65f);
        int heading=int(std::remainder(camera.yaw*57.29578f+360.f,360.f));if(heading<0)heading+=360;
        text(width*.5f-132,19,U"АЗИМУТ  "+number(heading)+U" град.   БОЄЦЬ "+number(soldier.id+1),25,0xd8dacc);
        if(paused){rect(width*.5f-65,60,130,35,0x151d22);text(width*.5f-43,63,U"ПАУЗА",28,0xe1c283);}
        const float cx=width*.5f,cy=height*.5f;
        if(!sim.directInput().aim){rect(cx-12,cy-1,7,2,0xe1e2d6,1);rect(cx+5,cy-1,7,2,0xe1e2d6,1);rect(cx-1,cy-12,2,7,0xe1e2d6,1);rect(cx-1,cy+5,2,7,0xe1e2d6,1);}else rect(cx-1,cy-1,2,2,0xe1e2d6,1);
        if(sim.lastShot().hit&&sim.ticks()-sim.lastShot().tick<3){rect(cx-6,cy-6,3,3,0xe5ba78,1);rect(cx+3,cy+3,3,3,0xe5ba78,1);rect(cx+3,cy-6,3,3,0xe5ba78,1);rect(cx-6,cy+3,3,3,0xe5ba78,1);}
        rect(22,height-112,228,90,0x11191f,.82f);text(36,height-104,U"ЗДОРОВ'Я  "+number(soldier.health),25,0xe4e5d9);
        rect(36,height-69,194,6,0x303b3b);rect(36,height-69,194*std::max(0,int(soldier.health))/100.f,6,soldier.health<30?0xc86452:0x9bb596,1);
        text(36,height-58,U"БРОНЯ  "+number(soldier.armor),22,0x9fb9c7);
        rect(width-250,height-112,228,90,0x11191f,.82f);text(width-232,height-105,utf8(weapon.name),25,0xddc595);
        text(width-232,height-76,number(soldier.magazine)+U" / "+number(std::max(0,int(soldier.ammo)-int(soldier.magazine))),34);
        if(soldier.reloadTicks)text(width-232,height-41,U"ПЕРЕЗАРЯДЖАННЯ",19,0xe1c283);
        if(width>=900)text(width*.5f-196,height-45,U"WASD рух · ЛКМ вогонь · ПКМ приціл · R зарядити",20,0xdbddcd);
        label(22,18,124,34,U"ТАКТИКА 1",{HudAction::Tactical});
        if(soldier.health<=0)text(cx-140,cy-65,U"БОЄЦЬ ЗАГИНУВ · TAB НАСТУПНИЙ",27,0xeaad9f);
        if(soldier.health<30){rect(0,0,width,5,0xb3453c,.8f);rect(0,height-5,width,5,0xb3453c,.8f);}
        return;
    }
    // Compact metal-framed command interface: resources/time above, army below.
    rect(0,0,width,54,0x171e25,.98f);rect(0,52,width,2,0x88774e,1);
    text(18,7,U"GRAY ZONE",32,0xe0cb9d);
    text(210,9,U"СВОЇ "+number(stats.alivePlayer)+U"     ВТРАТИ "+number(stats.casualties),25);
    if(campaign&&width>=850)text(520,9,U"БЮДЖЕТ "+number(std::max(0,campaign->funds)),25,0xd8c696);
    label(width-155,9,140,35,paused?U"ПРОДОВЖИТИ":U"ПАУЗА",{HudAction::Pause},paused);
    if(campaign){const auto& c=campaign->contract;
        rect(16,68,std::min(500,width-32),63,0x182128,.91f);rect(16,68,3,63,0xb7a16a);
        text(29,72,c.stage==ContractStage::Victory?U"КОНТРАКТ ВИКОНАНО":c.stage==ContractStage::Defeat?U"ОПЕРАЦІЮ ЗІРВАНО":U"ОПЕРАЦІЯ: КОНТРОЛЬ ПЕРЕПРАВИ",25,0xdfcfaa);
        text(29,104,c.stage==ContractStage::Engaged?U"Утримання "+number(c.holdTicks/10)+U" / 12 с · Час "+number(sim.ticks()/10)+U" с":utf8(campaign->latestNews).substr(0,68),21);
    }
    if(width<700)return;
    const float bottom=height-88.f;
    rect(0,bottom,width,88,0x131b23,.96f);rect(0,bottom,width,2,0x87794f);
    const float left=16,sideY=153;
    rect(left,sideY,214,205,0x182128,.91f);rect(left,sideY,214,2,0x8e7f5b);
    text(left+12,sideY+9,U"ВІДДІЛЕННЯ "+number(selected+1),27,0xdfcfaa);
    if(selected>=0&&std::size_t(selected)<sim.squadCount()){
        const auto& sq=sim.squad(std::size_t(selected));unsigned alive=0,health=0,armor=0,ammo=0;
        for(std::size_t i=sq.memberStart;i<sq.memberStart+sq.memberCount;++i){const auto& s=sim.soldier(i);if(s.health>0){++alive;health+=s.health;armor+=s.armor;ammo+=s.ammo;}}
        text(left+12,sideY+44,U"Бійці "+number(alive)+U" / "+number(sq.memberCount),24);
        text(left+12,sideY+75,U"Здоров'я "+number(health/std::max(1u,alive))+U"  Броня "+number(armor/std::max(1u,alive)),21);
        text(left+12,sideY+102,U"Набої "+number(ammo),22,0xaec1c4);
    }
    label(left+12,sideY+138,190,32,U"ВІД ПЕРШОЇ ОСОБИ 2",{HudAction::FirstPerson});
    label(left+12,sideY+175,91,28,U"УТРИМАТИ",{HudAction::Hold});
    label(left+111,sideY+175,91,28,U"МАПА 3",{HudAction::Map});
    if(campaign&&campaign->contract.stage==ContractStage::Planning)label(16,372,214,38,U"ВИСАДКА — ENTER",{HudAction::Deploy},true);
    if(campaign&&(campaign->contract.stage==ContractStage::Victory||campaign->contract.stage==ContractStage::Defeat))label(16,372,214,38,U"НОВИЙ КОНТРАКТ",{HudAction::Repeat},true);
    int cards=0;std::vector<std::size_t> friendly;for(std::size_t i=0;i<sim.squadCount();++i)if(sim.squad(i).side==Side::Player)friendly.push_back(i);
    auto chosen=std::find(friendly.begin(),friendly.end(),std::size_t(std::max(0,selected)));const std::size_t pageSize=std::size_t(std::clamp((width-172)/109,1,6));std::size_t first=chosen==friendly.end()?0:std::size_t(chosen-friendly.begin())/pageSize*pageSize;
    for(std::size_t n=first;n<friendly.size()&&cards<6;++n){const auto i=friendly[n];const auto& sq=sim.squad(i);unsigned alive=0,health=0;
        for(std::size_t j=sq.memberStart;j<sq.memberStart+sq.memberCount;++j)if(sim.soldier(j).health>0){++alive;health+=sim.soldier(j).health;}
        const float x=16+cards*109.f;if(x+103>width-155)break;
        label(x,bottom+12,101,56,U"ЗАГІН "+number(i+1),{HudAction::Select,int(i)},int(i)==selected);
        text(x+9,bottom+48,number(alive)+U" бійців",19);rect(x+8,bottom+72,84*float(health)/std::max(1u,sq.memberCount*100),4,0x9cb890,1);++cards;
    }
    label(width-145,bottom+15,128,30,quality==2?U"ЯКІСТЬ ВИСОКА":quality==1?U"ЯКІСТЬ СЕРЕДНЯ":U"ЯКІСТЬ НИЗЬКА",{HudAction::Quality});
    text(width-145,bottom+54,U"ПКМ наказ · TAB",18,0xacb8be);
    const float mapSize=std::clamp(width*.17f,132.f,216.f),mx=width-mapSize-18,my=70;
    if(my+mapSize<bottom-15){
        rect(mx-4,my-4,mapSize+8,mapSize+30,0x19242b);text(mx+5,my+mapSize+3,U"ТАКТИЧНА МАПА",19,0xcdc8b0);
        const float range=1000.f;
        for(int z=0;z<40;++z)for(int x=0;x<40;++x){const float wx=camera.focusX+(x/39.f-.5f)*range,wz=camera.focusZ+(z/39.f-.5f)*range,h=terrain.heightAt(wx,wz);
            const float slope=(terrain.heightAt(wx+8,wz)-h)/8;const auto mask=TerrainGenerator::surface(wx,wz,h,slope,terrain.generatorSettings());
            std::uint32_t color=mask.water>.1f?0x325569:mask.road>.3f?0x9c9276:mask.wetness>.4f?0x4d5747:0x59694d;
            rect(mx+x*mapSize/40,my+z*mapSize/40,mapSize/40+1,mapSize/40+1,color,1);
        }
        for(std::size_t i=0;i<sim.soldierCount();++i){const auto& s=sim.soldier(i);if(s.health<=0)continue;const bool player=sim.squad(s.squad).side==Side::Player;
            if(!player&&std::hypot(s.x-camera.focusX,s.z-camera.focusZ)>sim.visibilityMeters())continue;
            const float x=(s.x-camera.focusX)/range+.5f,z=(s.z-camera.focusZ)/range+.5f;
            if(x>=0&&x<=1&&z>=0&&z<=1)rect(mx+x*mapSize-1,my+z*mapSize-1,3,3,player?0xb5d1be:0xdf8371,1);
        }
        const float ox=(3000-camera.focusX)/range+.5f,oz=(1150-camera.focusZ)/range+.5f;
        if(ox>=0&&ox<=1&&oz>=0&&oz<=1)rect(mx+ox*mapSize-4,my+oz*mapSize-4,8,8,0xdac082,1);
        buttons_.push_back({mx,my,mapSize,mapSize,{HudAction::Focus,0,camera.focusX,camera.focusZ}});
    }
    if(selected>=0&&std::size_t(selected)<sim.squadCount()){
        const auto frame=makeGpuFrameConstants(makeGpuSceneCamera(camera,terrain.heightAt(camera.focusX,camera.focusZ),{}),width,height,0);const auto& sq=sim.squad(selected);
        for(std::size_t i=sq.memberStart;i<sq.memberStart+sq.memberCount;++i){const auto& s=sim.soldier(i);float x=0,y=0;if(s.health>0&&projectGpuPoint(frame,width,height,s.x,terrain.heightAt(s.x,s.z)+2.1f,s.z,x,y)&&x>240&&x<width-mapSize-30&&y>140&&y<bottom)rect(x-4,y-4,8,3,0xe0c891,1);}
    }
}
void TacticalHud::drawCpu(std::uint32_t* pixels,int width,int height)const{
    if(!pixels||width!=width_||height!=height_)return;
    for(std::size_t i=0;i+3<vertices_.size();i+=4){const auto& a=vertices_[i];const auto& b=vertices_[i+2];
        const int x0=std::clamp(int(a.x),0,width),x1=std::clamp(int(std::ceil(b.x)),0,width),y0=std::clamp(int(a.y),0,height),y1=std::clamp(int(std::ceil(b.y)),0,height);
        for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x){float alpha=a.a;
            if(a.u>=0){const float u=a.u+(b.u-a.u)*(x+.5f-a.x)/std::max(1.f,b.x-a.x),v=a.v+(b.v-a.v)*(y+.5f-a.y)/std::max(1.f,b.y-a.y);
                const int sx=std::clamp(int(u*glyphWidth_),0,glyphWidth_-1),sy=std::clamp(int(v*glyphHeight_),0,glyphHeight_-1);alpha*=glyphs_[std::size_t(sy)*glyphWidth_+sx]/255.f;}
            auto& p=pixels[std::size_t(y)*width+x];unsigned r=unsigned(((p>>16)&255)*(1-alpha)+a.r*255*alpha),g=unsigned(((p>>8)&255)*(1-alpha)+a.g*255*alpha),bl=unsigned((p&255)*(1-alpha)+a.b*255*alpha);p=(r<<16)|(g<<8)|bl;
        }
    }
}
HudCommand TacticalHud::hit(float x,float y)const{
    for(auto it=buttons_.rbegin();it!=buttons_.rend();++it)if(x>=it->x&&x<=it->x+it->w&&y>=it->y&&y<=it->y+it->h){auto command=it->command;
        if(command.action==HudAction::Focus){command.worldX+=(x-it->x)/it->w*1000.f-500.f;command.worldZ+=(y-it->y)/it->h*1000.f-500.f;}return command;}
    return {};
}
bool TacticalHud::contains(float x,float y)const{return hit(x,y).action!=HudAction::None||(y>=0&&y<=54)||(y>=height_-88&&y<height_)||(x>=16&&x<=230&&y>=153&&y<=358);}
}
