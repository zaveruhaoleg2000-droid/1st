#include "gz/Diagnostics.hpp"
#include "gz/Glyphs.hpp"
#include "gz/Campaign.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
namespace gz {
void FrameMeter::record(float sceneMs,float frameMs)noexcept{
    if(!std::isfinite(sceneMs)||!std::isfinite(frameMs)||sceneMs<0||frameMs<=0)return;
    sceneMs=std::min(sceneMs,60000.f);frameMs=std::min(frameMs,60000.f);
    if(count_==samples_.size()){
        sumScene_-=samples_[cursor_].scene;sumFrame_-=samples_[cursor_].frame;
    }else ++count_;
    samples_[cursor_]={sceneMs,frameMs};
    sumScene_+=sceneMs;sumFrame_+=frameMs;
    cursor_=(cursor_+1)%samples_.size();
}
DiagnosticSnapshot FrameMeter::snapshot(std::uint64_t triangles,std::size_t cacheBytes,
                                       std::uint64_t uploadedBytes,const char* renderer)const{
    DiagnosticSnapshot s;
    if(count_){s.sceneMs=float(sumScene_/double(count_));s.frameMs=float(sumFrame_/double(count_));s.fps=s.frameMs>0.f?1000.f/s.frameMs:0.f;}
    s.triangles=triangles;s.terrainCacheBytes=cacheBytes;s.uploadedBytes=uploadedBytes;
    s.renderer=renderer?renderer:"unknown";return s;
}
bool DebugHud::load(const std::string& glyphPgm){
    glyphs_.clear();glyphW_=glyphH_=0;
    std::ifstream in(glyphPgm,std::ios::binary);std::string magic;int max=0,w=0,h=0;
    if(!(in>>magic>>w>>h>>max)||magic!="P5"||max!=255||w<kGlyphCols*kGlyphCellW||w>4096||h<kGlyphCellH||h>4096)return false;
    in.get();std::vector<std::uint8_t> loaded(std::size_t(w)*h);
    in.read(reinterpret_cast<char*>(loaded.data()),std::streamsize(loaded.size()));
    if(!in)return false;
    glyphW_=w;glyphH_=h;glyphs_=std::move(loaded);return true;
}
void DebugHud::write(std::uint32_t* pixels,int w,int h,int x,int y,const std::string& text,std::uint32_t color)const{
    constexpr int dw=7,dh=10;
    for(unsigned char ch:text){
        std::size_t idx=0;
        while(kMenuGlyphs[idx]&&kMenuGlyphs[idx]!=char32_t(ch))++idx;
        if(!kMenuGlyphs[idx])idx=0;
        const int gx=int(idx%kGlyphCols)*kGlyphCellW,gy=int(idx/kGlyphCols)*kGlyphCellH;
        if(gx+kGlyphCellW>glyphW_||gy+kGlyphCellH>glyphH_){x+=dw;continue;}
        for(int dy=0;dy<dh;++dy){int sy=gy+dy*kGlyphCellH/dh;
            const int py=y+dy;if(py<0||py>=h)continue;
            for(int dx=0;dx<dw;++dx){int px=x+dx;if(px<0||px>=w)continue;
                const int srcX=gx+dx*kGlyphCellW/dw;
                const auto ink=glyphs_[std::size_t(sy)*glyphW_+srcX];
                if(ink>105)pixels[std::size_t(py)*w+px]=color;
            }
        }
        x+=dw;
    }
}
void DebugHud::draw(std::uint32_t* pixels,int w,int h,const DiagnosticSnapshot& s)const{
    if(!pixels||!ready()||w<240||h<180)return;
    const int panelW=std::min(w-16,254),panelX=12,panelY=std::min(h-100,102);
    for(int y=panelY;y<std::min(panelY+86,h);++y){
        for(int x=panelX;x<std::min(panelX+panelW,w);++x){
            auto& px=pixels[std::size_t(y)*w+x];
            const auto r=unsigned((px>>16)&255u),g=unsigned((px>>8)&255u),b=unsigned(px&255u);
            px=0xff000000u|((r*32/100+7u)<<16)|((g*32/100+13u)<<8)|(b*32/100+19u);
        }
    }
    for(int y=panelY;y<std::min(panelY+86,h);++y){pixels[std::size_t(y)*w+panelX]=0xffd0b778u;}
    auto fmt=[](float n,int decimals=1){std::ostringstream o;o<<std::fixed<<std::setprecision(decimals)<<n;return o.str();};
    write(pixels,w,h,panelX+8,panelY+5,"GZ89 DEBUG | " + s.renderer,0xffe4d5aau);
    write(pixels,w,h,panelX+8,panelY+22,"RENDER FPS " + fmt(s.fps)+"  FRAME "+fmt(s.frameMs),0xffc9ebd6u);
    write(pixels,w,h,panelX+8,panelY+39,"SCENE " + fmt(s.sceneMs)+" MS",0xffd3d9e3u);
    write(pixels,w,h,panelX+8,panelY+56,"TRI " + std::to_string(s.triangles)+"  CACHE "+fmt(float(s.terrainCacheBytes)/(1024*1024))+" MB",0xffd3d9e3u);
    write(pixels,w,h,panelX+8,panelY+72,"GPU UPLOAD " + fmt(float(s.uploadedBytes)/(1024*1024))+" MB",0xffd3d9e3u);
}
}

namespace gz {
void DebugHud::drawContract(std::uint32_t* pixels,int w,int h,const CampaignState& s)const {
    if(!pixels||!ready()||w<300||h<200||s.scenarioId.empty())return;
    const auto& c=s.contract;
    #ifdef _WIN32
    const int panelY=std::min(h-78,145); // GDI overlays the first 134 screen pixels.
#else
    const int panelY=28;
#endif
    const int x=12,y=panelY,panelW=std::min(w-24,510),panelH=72;
    for(int py=y;py<y+panelH&&py<h;++py)for(int px=x;px<x+panelW&&px<w;++px){
        auto& dst=pixels[std::size_t(py)*w+px];
        const unsigned r=(dst>>16)&255u,g=(dst>>8)&255u,b=dst&255u;
        dst=0xff000000u|((r*15/100+12)<<16)|((g*15/100+21)<<8)|(b*15/100+30);
    }
    const std::uint32_t gold=0xffdfb96au,cyan=0xffc6e5e2u;
    for(int py=y;py<y+panelH&&py<h;++py)pixels[std::size_t(py)*w+x]=gold;
    write(pixels,w,h,x+8,y+5,"GRAY ZONE: 1989 | "+std::string(contractStageName(c.stage)),gold);
    if(c.stage==ContractStage::Planning){
        write(pixels,w,h,x+8,y+20,"CONTRACT: "+s.scenarioId,cyan);
        write(pixels,w,h,x+8,y+35,"TROOPS "+std::to_string(c.strength)+" | COST "+std::to_string(ContractDirector::deploymentCost(c.strength))+" | CASH "+std::to_string(s.funds),cyan);
        write(pixels,w,h,x+8,y+50,"J/K: TROOPS  ENTER: DEPLOY  3: MAP",gold);
    }else if(c.stage==ContractStage::Engaged){
        write(pixels,w,h,x+8,y+20,"OBJECTIVE: CONTROL THE CROSSING",cyan);
        write(pixels,w,h,x+8,y+35,"CAPTURE "+std::to_string(c.holdTicks)+"/120 TICKS  LOSSES "+std::to_string(c.friendlyLosses),cyan);
        write(pixels,w,h,x+8,y+50,"1: COMMAND  2: DIRECT  3: GLOBAL  T: REALTIME",gold);
    }else{
        write(pixels,w,h,x+8,y+20,"LOSSES "+std::to_string(c.friendlyLosses)+" | PAYOUT "+std::to_string(c.lastReward),cyan);
        write(pixels,w,h,x+8,y+35,"LOCAL INFLUENCE "+std::to_string(c.sectorInfluence)+" | CASH "+std::to_string(s.funds),cyan);
        write(pixels,w,h,x+8,y+50,"N: NEW CONTRACT   F5: SAVE   F9: LOAD",gold);
    }
}
}
