#include "gz/Menu.hpp"
#include "gz/Glyphs.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
namespace gz {
namespace {
std::uint32_t rgb(unsigned r,unsigned g,unsigned b){return ((r&255u)<<16)|((g&255u)<<8)|(b&255u);}
std::uint32_t mix(std::uint32_t a,std::uint32_t b,float t){t=std::clamp(t,0.f,1.f);
  return rgb(unsigned(((a>>16)&255u)*(1-t)+((b>>16)&255u)*t),unsigned(((a>>8)&255u)*(1-t)+((b>>8)&255u)*t),unsigned((a&255u)*(1-t)+(b&255u)*t));}
}
int MenuState::rows()const{return page==MenuPage::Playing?0:4;}
void MenuState::move(int delta){if(rows()>0)selected=(selected+delta+rows()*10)%rows();}
void MenuState::back(){if(page==MenuPage::Load||page==MenuPage::Settings||page==MenuPage::Scenario){page=MenuPage::Main;selected=0;}}
std::u32string MenuState::label(int i)const{
  if(i<0||i>=4)return {};
  switch(page){
  case MenuPage::Main:{static const std::u32string labels[]={U"Нова гра",U"Завантаження збережень",U"Налаштування",U"Вихід"};return labels[i];}
  case MenuPage::Load:{static const std::u32string labels[]={U"Слот 01",U"Слот 02",U"Слот 03",U"Назад"};
    if(i==3)return labels[3];
    return labels[i]+(saveExists[std::size_t(i)]?U"  — збережено":U"  — порожньо");}
  case MenuPage::Settings:{if(i==0)return aa?U"Згладжування      УВІМК":U"Згладжування      ВИМК";
    if(i==1)return wind?U"Вітер                     УВІМК":U"Вітер                     ВИМК";
    if(i==2)return water?U"Відображення води  УВІМК":U"Відображення води  ВИМК";
    return U"Назад";}
  case MenuPage::Scenario:{static const std::u32string labels[]={U"Варшавський коридор",U"Лінія Прип'яті",U"Дунайський архів",U"Назад"};return labels[i];}
  case MenuPage::Playing:return {};
  }return {};
}
MenuAction MenuState::activate(int i){
  if(i<0||i>=rows())return MenuAction::None;
  selected=i;
  if(page==MenuPage::Main){
    if(i==0){page=MenuPage::Scenario;selected=0;return MenuAction::None;}
    if(i==1){page=MenuPage::Load;selected=0;return MenuAction::None;}
    if(i==2){page=MenuPage::Settings;selected=0;return MenuAction::None;}
    return MenuAction::Exit;
  }
  if(i==3){back();return MenuAction::None;}
  if(page==MenuPage::Scenario){page=MenuPage::Playing;return MenuAction(int(MenuAction::StartScenario1)+i);}
  if(page==MenuPage::Load){if(!saveExists[std::size_t(i)])return MenuAction::None;
    page=MenuPage::Playing;return MenuAction(int(MenuAction::LoadSlot1)+i);
  }
  if(page==MenuPage::Settings){
    if(i==0)aa=!aa;else if(i==1)wind=!wind;else water=!water;
    return MenuAction::SettingsChanged;
  }
  return MenuAction::None;
}
int MenuState::hitRow(int x,int y,int w,int h){
  int left=int(w*.036f),right=left+int(w*.39f);
  int top=int(h*.392f),step=std::max(41,int(h*.087f)),height=std::max(36,int(step*.82f));
  if(x<left||x>=right)return -1;
  for(int i=0;i<4;++i)if(y>=top+i*step && y<top+i*step+height)return i;
  return -1;
}
bool MenuScene::load(const std::string& mapPath,const std::string& glyphPath,const std::string& advancePath){
  bool map=map_.load(mapPath);
  if(map)map_.loadPolitics("assets/politics/political_1989.gzp");
  std::ifstream in(glyphPath,std::ios::binary);
  std::string magic;int maxval=0;
  if(in && (in>>magic>>glyphW_>>glyphH_>>maxval) && magic=="P5" && maxval==255 && glyphW_>0 && glyphH_>0 && glyphW_<4096 && glyphH_<4096){
    in.get();glyphs_.resize(std::size_t(glyphW_)*glyphH_);
    in.read(reinterpret_cast<char*>(glyphs_.data()),std::streamsize(glyphs_.size()));
    if(!in)glyphs_.clear();
  }
  std::ifstream adv(advancePath,std::ios::binary);
  if(adv){advances_.assign(std::istreambuf_iterator<char>(adv),std::istreambuf_iterator<char>());}
  auto loadAtlas=[](const char* filename,std::size_t bytes,std::vector<std::uint8_t>& target){
    std::ifstream f(filename,std::ios::binary);
    if(!f)return;
    target.assign(std::istreambuf_iterator<char>(f),{});
    if(target.size()!=bytes)target.clear();
  };
  loadAtlas("assets/ui/pmc_flags.rgb",6*112*4*80*3,flags_);
  loadAtlas("assets/ui/pmc_portraits.rgb",6*88*4*96*3,portraits_);
  return map && !glyphs_.empty() && advances_.size()==sizeof(kMenuGlyphs)/sizeof(char32_t)-1;
}
void MenuScene::atlasTile(const std::vector<std::uint8_t>& source,int tw,int th,int index,int x,int y,int zoom){
  if(source.empty()||index<0||index>=24||zoom<=0)return;
  const int fullW=tw*6,ox=(index%6)*tw,oy=(index/6)*th;
  for(int yy=0;yy<th*zoom;++yy){int py=y+yy;if(py<0||py>=h_)continue;
    for(int xx=0;xx<tw*zoom;++xx){int px=x+xx;if(px<0||px>=w_)continue;
      const std::size_t i=(std::size_t(oy+yy/zoom)*fullW+ox+xx/zoom)*3;
      buffer_[std::size_t(py)*w_+px]=rgb(source[i],source[i+1],source[i+2]);
    }
  }
}
void MenuScene::rect(int x,int y,int width,int height,std::uint32_t c,float alpha){
  int x0=std::clamp(x,0,w_),x1=std::clamp(x+width,0,w_),y0=std::clamp(y,0,h_),y1=std::clamp(y+height,0,h_);
  for(int yy=y0;yy<y1;++yy)for(int xx=x0;xx<x1;++xx){auto& p=buffer_[std::size_t(yy)*w_+xx];p=mix(p,c,alpha);}
}
void MenuScene::border(int x,int y,int width,int height,std::uint32_t c,int size){
  rect(x,y,width,size,c);rect(x,y+height-size,width,size,c);
  rect(x,y,size,height,c);rect(x+width-size,y,size,height,c);
}
void MenuScene::text(int x,int y,std::u32string_view s,float scale,std::uint32_t color){
  if(glyphs_.empty()||scale<=0)return;
  int cursor=x;
  for(char32_t ch:s){
    std::size_t idx=0;
    while(kMenuGlyphs[idx] && kMenuGlyphs[idx]!=ch)++idx;
    if(!kMenuGlyphs[idx])idx=1;
    const int sourceX=int(idx%kGlyphCols)*kGlyphCellW,sourceY=int(idx/kGlyphCols)*kGlyphCellH;
    const int dx=std::max(1,int(kGlyphCellW*scale)),dy=std::max(1,int(kGlyphCellH*scale));
    for(int yy=0;yy<dy;++yy){int py=y+yy;if(py<0||py>=h_)continue;
      int sy=sourceY+std::min(kGlyphCellH-1,int(yy/scale));
      for(int xx=0;xx<dx;++xx){int px=cursor+xx;if(px<0||px>=w_)continue;
        int sx=sourceX+std::min(kGlyphCellW-1,int(xx/scale));
        int ink=glyphs_[std::size_t(sy)*glyphW_+sx];if(ink<20)continue;
        auto& pixel=buffer_[std::size_t(py)*w_+px];pixel=mix(pixel,color,ink/255.f);
      }
    }
    cursor+=std::max(4,int((idx<advances_.size()?advances_[idx]:25)*scale));
  }
}
void MenuScene::draw(const MenuState& menu,int w,int h,float seconds){
  w_=std::clamp(w,320,1600);h_=std::clamp(h,240,1000);
  map_.render(w_,h_,buffer_,seconds);
  auto white=rgb(221,224,211),muted=rgb(149,167,168),gold=rgb(218,201,142),red=rgb(177,73,66);
  const float s=std::min(w_/1136.f,h_/640.f);
  // Main panel and subtle top/bottom scanlines: all generated by the running engine.
  int side=int(w_*.395f);
  rect(0,0,side,h_,rgb(9,19,25),.69f);
  rect(side-2,0,2,h_,rgb(102,126,130),.31f);
  rect(0,0,w_,3,rgb(155,177,164),.22f);
  text(int(w_*.036f),int(h_*.107f),U"GRAY ZONE:",std::clamp(1.8f*s,.7f,2.2f),white);
  text(int(w_*.036f),int(h_*.200f),U"1989",std::clamp(2.65f*s,1.1f,3.f),red);
  rect(int(w_*.037f),int(h_*.347f),int(w_*.309f),2,gold,.62f);
  const int left=int(w_*.036f),top=int(h_*.392f),step=std::max(41,int(h_*.087f));
  const int width=int(w_*.39f),height=std::max(36,int(step*.82f));
  for(int i=0;i<menu.rows();++i){
    int y=top+step*i;bool selected=menu.selected==i;
    if(selected){rect(left,y,width,height,rgb(93,96,61),.44f);border(left,y,width,height,gold,1);
      text(left+width-int(33*s),y+int(7*s),U">",.98f*s,gold);
    }else {rect(left,y,width,height,rgb(7,19,26),.45f);rect(left,y+height-1,width,1,rgb(102,124,119),.4f);}
    std::uint32_t textColor=selected?rgb(241,225,165):white;
    if(menu.page==MenuPage::Load && i<3 && !menu.saveExists[std::size_t(i)])textColor=muted;
    // More compact line in settings: the visual menu remains usable at 960x600.
    float labelSize=(menu.page==MenuPage::Settings?.61f:(menu.page==MenuPage::Load?.67f:.78f))*s;
    text(left+int(19*s),y+int(5*s),menu.label(i),labelSize,textColor);
  }
  const auto mapLabel=menu.page==MenuPage::Main?U"ЄВРОПА  /  1989":menu.page==MenuPage::Load?U"ЗБЕРЕЖЕННЯ ОПЕРАЦІЇ":menu.page==MenuPage::Scenario?U"ПОЧАТОК КАМПАНІЇ":U"НАЛАШТУВАННЯ РУШІЯ";
  text(int(w_*.036f),int(h_*.750f),mapLabel,.59f*s,muted);
  if(menu.page==MenuPage::Load)text(int(w_*.036f),int(h_*.80f),U"F5 — зберегти у слот 01",.50f*s,muted);
  else if(menu.page==MenuPage::Scenario)text(int(w_*.036f),int(h_*.80f),U"3 кампанії  /  8 фракцій  /  24 ЧВК",.46f*s,muted);
  else if(menu.page==MenuPage::Settings)text(int(w_*.036f),int(h_*.80f),U"Ефекти працюють у 3D-режимі",.46f*s,muted);
  else text(int(w_*.036f),int(h_*.80f),U"ПОЛЬЩА  •  ЧВК  •  СХІДНИЙ ФРОНТ",.49f*s,muted);
  rect(int(w_*.034f),int(h_*.866f),int(w_*.316f),1,gold,.38f);
  text(int(w_*.036f),int(h_*.905f),U"GRAY ZONE  /  v0.0.25",.42f*s,muted);
  if(menu.page==MenuPage::Scenario){
    const int index=std::clamp(menu.selected,0,2);
    static const std::u32string titles[]={U"ВАРШАВА / 18.03.1989",U"ПРИП'ЯТЬ / 20.03.1989",U"БУДАПЕШТ / 22.03.1989"};
    static const std::u32string employers[]={U"ЗАМОВНИК: ПОЛЬЩА",U"ЗАМОВНИК: УКРАЇНСЬКА РСР",U"ЗАМОВНИК: УГОРЩИНА"};
    static const std::u32string descriptions[]={U"ЗНИКЛИЙ ЕШЕЛОН / ЗАХИСТ МОСТУ",U"ПРОВАЛ ЗВ'ЯЗКУ / НЕЙТРАЛЬНИЙ КОРИДОР",U"ТАЄМНІ ПЛАТЕЖІ / АРХІВ"};
    const int bx=int(w_*.48f),by=int(h_*.55f),bw=int(w_*.50f);
    rect(bx,by,bw,int(h_*.26f),rgb(7,15,20),.88f);
    border(bx,by,bw,int(h_*.26f),rgb(149,151,124));
    const int pmc=index==0?0:(index==1?8:20);
    atlasTile(flags_,112,80,pmc,bx+12,by+12,1);
    atlasTile(portraits_,88,96,pmc,bx+145,by+10,1);
    text(bx+12,by+98,titles[index],.47f*s,gold);
    text(bx+12,by+125,employers[index],.39f*s,white);
    text(bx+12,by+149,descriptions[index],.32f*s,muted);
  }
  text(int(w_*.757f),int(h_*.935f),menu.page==MenuPage::Scenario?U"ВИБІР СЦЕНАРІЮ":U"СТРАТЕГІЧНА МАПА",.46f*s,muted);
}
bool MenuScene::savePPM(const std::string& path)const{
  std::ofstream out(path,std::ios::binary);if(!out||buffer_.empty())return false;
  out<<"P6\n"<<w_<<" "<<h_<<"\n255\n";
  for(auto p:buffer_){char v[3]={char((p>>16)&255),char((p>>8)&255),char(p&255)};out.write(v,3);}
  return bool(out);
}
}
