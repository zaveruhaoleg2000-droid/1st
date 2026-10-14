from pathlib import Path
R=Path(__file__).resolve().parents[1]
h=R/'include/gz/Europe.hpp';s=h.read_text()
s=s.replace('    bool loaded() const {return !vertices_.empty();}', '    bool loaded() const {return !vertices_.empty();}\n    bool loadPolitics(const std::string& path,const std::string& glyphPath="assets/ui/glyphs.pgm");\n    bool politicsLoaded() const {return political_.size()==vertices_.size() && !political_.empty();}\n    std::uint8_t politicalAt(float lon,float lat)const;')
s=s.replace('    void render(int width,int height,std::vector<std::uint32_t>& output,float seconds=0.f,bool strategic=false);','    void render(int width,int height,std::vector<std::uint32_t>& output,float seconds=0.f,bool strategic=false,bool political=true);')
s=s.replace('    std::vector<Vertex> vertices_;','    void glyphText(int x,int y,std::u32string_view text,float scale,std::uint32_t color);\n    std::vector<std::uint8_t> political_,glyphs_;\n    int fontW_=0,fontH_=0;\n    std::vector<Vertex> vertices_;')
s=s.replace('#include <string>','#include <string>\n#include <string_view>')
h.write_text(s)
p=R/'src/Europe.cpp';s=p.read_text()
s=s.replace('#include "gz/Europe.hpp"','#include "gz/Europe.hpp"\n#include "gz/Glyphs.hpp"')
s=s.replace('namespace {\nstd::uint32_t rgb', '''namespace {
static constexpr std::array<std::uint32_t,16> kPolitics={
  0x112838u,0x386985u,0x794e55u,0x763939u,0x747f6au,0x766d51u,0xb68a50u,0x89644bu,
  0x765758u,0x705c81u,0x9b6c65u,0x8d4d58u,0x457a9au,0x86627fu,0x795866u,0x85545eu};
std::uint32_t rgb''')
insert='''
bool EuropeBackdrop::loadPolitics(const std::string& path,const std::string& glyphPath){
  political_.clear();glyphs_.clear();fontW_=fontH_=0;
  if(vertices_.empty())return false;
  std::ifstream f(path,std::ios::binary);if(!f)return false;
  char magic[4]{};f.read(magic,4);
  unsigned char dims[4]{};f.read(reinterpret_cast<char*>(dims),4);
  const int w=int(dims[0])+(int(dims[1])<<8),h=int(dims[2])+(int(dims[3])<<8);
  if(!f||std::string(magic,4)!="GZP1"||w!=sourceW_||h!=sourceH_)return false;
  political_.resize(vertices_.size());f.read(reinterpret_cast<char*>(political_.data()),std::streamsize(political_.size()));
  if(!f||std::any_of(political_.begin(),political_.end(),[](std::uint8_t a){return a>15;})){
    political_.clear();return false;
  }
  std::ifstream font(glyphPath,std::ios::binary);std::string ft;int max=0;
  if(font && (font>>ft>>fontW_>>fontH_>>max) && ft=="P5" && max==255 &&fontW_>0&&fontW_<4096&&fontH_>0&&fontH_<4096){
    font.get();glyphs_.resize(std::size_t(fontW_)*fontH_);
    font.read(reinterpret_cast<char*>(glyphs_.data()),std::streamsize(glyphs_.size()));
    if(!font)glyphs_.clear();
  }
  return true;
}
std::uint8_t EuropeBackdrop::politicalAt(float lon,float lat)const{
  if(!politicsLoaded()||lon<west_||lon>east_||lat<south_||lat>north_)return 0;
  int x=std::clamp(int(std::round((lon-west_)/(east_-west_)*(sourceW_-1))),0,sourceW_-1);
  int y=std::clamp(int(std::round((north_-lat)/(north_-south_)*(sourceH_-1))),0,sourceH_-1);
  return political_[std::size_t(y)*sourceW_+x];
}
void EuropeBackdrop::glyphText(int x,int y,std::u32string_view text,float scale,std::uint32_t c){
  if(glyphs_.empty())return;
  int cursor=x;
  for(char32_t ch:text){
    std::size_t idx=0;while(kMenuGlyphs[idx]&&kMenuGlyphs[idx]!=ch)++idx;
    if(!kMenuGlyphs[idx])idx=1;
    const int srcX=int(idx%kGlyphCols)*kGlyphCellW,srcY=int(idx/kGlyphCols)*kGlyphCellH;
    const int dw=std::max(1,int(kGlyphCellW*scale)),dh=std::max(1,int(kGlyphCellH*scale));
    for(int yy=0;yy<dh;++yy){int py=y+yy;if(py<0||py>=h_)continue;
      int sy=srcY+std::min(kGlyphCellH-1,int(yy/scale));
      for(int xx=0;xx<dw;++xx){int px=cursor+xx;if(px<0||px>=w_)continue;
        int sx=srcX+std::min(kGlyphCellW-1,int(xx/scale));
        int ink=glyphs_[std::size_t(sy)*fontW_+sx];if(ink<70)continue;
        auto& pix=(*pixels_)[std::size_t(py)*w_+px];pix=mix(pix,c,ink/255.f);
      }
    }
    cursor+=std::max(3,int(kGlyphCellW*scale*.63f));
  }
}
'''
s=s.replace('void EuropeBackdrop::point(',insert+'\nvoid EuropeBackdrop::point(')
s=s.replace('void EuropeBackdrop::render(int width,int height,std::vector<std::uint32_t>& output,float seconds,bool strategic){','void EuropeBackdrop::render(int width,int height,std::vector<std::uint32_t>& output,float seconds,bool strategic,bool political){')
s=s.replace('      p.color=rgb(s.r,s.g,s.b);','''      p.color=rgb(s.r,s.g,s.b);
      if(political && politicsLoaded() && s.land){
        const std::uint8_t faction=political_[std::size_t(y)*sourceW_+x];
        if(faction)p.color=mix(p.color,kPolitics[faction],strategic?.66f:.24f);
      }''')
s=s.replace('  // Quiet military grid; visually separates terrain from the UI while preserving readability.','''  // Politically relevant borders are read from the same geo-registered image as the mesh.
  // USSR republic boundaries are internal faction lines, not independent states in 1989.
  if(strategic&&political&&politicsLoaded()){
    for(int y=0;y<sourceH_-1;++y){for(int x=0;x<sourceW_-1;++x){
      const auto i=std::size_t(y)*sourceW_+x;
      const auto a=political_[i],b=political_[i+1],c=political_[i+sourceW_];
      const auto& pa=projected_[i];const auto& pb=projected_[i+1];const auto& pc=projected_[i+sourceW_];
      if(a && b && a!=b){
        const bool internal=(a==3||a==7||a==8||a==9)&&(b==3||b==7||b==8||b==9);
        if(!internal||(x+y)%3){auto& pd=projected_[i+sourceW_+1];
          line((pa.x+pb.x)*.5f,(pa.y+pb.y)*.5f,(pc.x+pd.x)*.5f,(pc.y+pd.y)*.5f,
          internal?rgb(205,167,146):rgb(238,220,174),internal?.45f:.88f);
        }
      }
      if(a && c && a!=c){
        const bool internal=(a==3||a==7||a==8||a==9)&&(c==3||c==7||c==8||c==9);
        if(!internal||(x+y)%3){auto& pd=projected_[i+sourceW_+1];
          line((pa.x+pc.x)*.5f,(pa.y+pc.y)*.5f,(pb.x+pd.x)*.5f,(pb.y+pd.y)*.5f,
          internal?rgb(205,167,146):rgb(238,220,174),internal?.45f:.88f);
        }
      }
    }}
  }
  // Quiet military grid; visually separates terrain from the UI while preserving readability.''')
needle='''  if(strategic){
    // On the strategic view make the operational area visible; the battle renderer is not reused as a fake world.'''
replace='''  if(strategic&&political&&politicsLoaded()){
    struct City{float lon,lat;std::u32string_view name;};
    constexpr City cities[]={
      {21.f,52.2f,U"ВАРШАВА"},{30.5f,50.45f,U"КИЇВ"},{27.56f,53.90f,U"МІНСЬК"},
      {19.05f,47.5f,U"БУДАПЕШТ"},{14.42f,50.08f,U"ПРАГА"},{13.4f,52.52f,U"БЕРЛІН"},
      {37.61f,55.75f,U"МОСКВА"},{26.1f,44.43f,U"БУХАРЕСТ"},
      {24.1f,56.95f,U"РИГА"},{2.35f,48.86f,U"ПАРИЖ"},{-0.12f,51.5f,U"ЛОНДОН"},
      {18.05f,59.3f,U"СТОКГОЛЬМ"}
    };
    for(const auto& city:cities){
      float u=(city.lon-west_)/(east_-west_),v=(north_-city.lat)/(north_-south_);
      const int xx=std::clamp(int(u*(sourceW_-1)),0,sourceW_-1),yy=std::clamp(int(v*(sourceH_-1)),0,sourceH_-1);
      const auto p=projected_[std::size_t(yy)*sourceW_+xx];
      if(p.x<3||p.x>w_-110||p.y<2||p.y>h_-25)continue;
      fill(int(p.x)-2,int(p.y)-2,5,5,rgb(231,208,157),.95f);
      glyphText(int(p.x)+6,int(p.y)-7,city.name,.275f*std::clamp(w_/960.f,.75f,1.6f),rgb(239,228,198));
    }
    fill(9,h_-57,321,49,rgb(8,17,23),.77f);
    glyphText(18,h_-51,U"ЄВРОПА / БЕРЕЗЕНЬ 1989",.36f*std::clamp(w_/960.f,.75f,1.5f),rgb(235,223,192));
    fill(17,h_-23,17,7,kPolitics[1]);glyphText(39,h_-27,U"НАТО",.26f,rgb(221,230,225));
    fill(104,h_-23,17,7,kPolitics[3]);glyphText(126,h_-27,U"СРСР",.26f,rgb(221,230,225));
    fill(194,h_-23,17,7,kPolitics[6]);glyphText(216,h_-27,U"ПОЛЬЩА",.26f,rgb(221,230,225));
  }
  if(strategic){
    // On the strategic view make the operational area visible; the battle renderer is not reused as a fake world.'''
if needle not in s:raise ValueError('needle not found')
s=s.replace(needle,replace)
p.write_text(s)
p=R/'src/Renderer.cpp';s=p.read_text().replace('if(!europe_.loaded())europe_.load("assets/europe/europe_vertices.gzm");','if(!europe_.loaded()){europe_.load("assets/europe/europe_vertices.gzm");europe_.loadPolitics("assets/politics/political_1989.gzp");}')
s=s.replace('europe_.render(w_,h_,color_,visualTime_,true);','europe_.render(w_,h_,color_,visualTime_,true,politicalMap_);')
p.write_text(s)
p=R/'include/gz/Renderer.hpp';s=p.read_text()
s=s.replace('    void setGlobalMapView(float zoom,float panX,float panY)', '    void setPoliticalMap(bool on){politicalMap_=on;}\n    bool politicalMap()const{return politicalMap_;}\n    void setGlobalMapView(float zoom,float panX,float panY)')
needle='    EuropeBackdrop europe_;';
print('Europe variable count',s.count(needle))
if needle in s:s=s.replace(needle,needle+'\n    bool politicalMap_=true;')
else:raise ValueError('renderer europe missing')
p.write_text(s)
p=R/'src/Menu.cpp';s=p.read_text().replace('  bool map=map_.load(mapPath);','  bool map=map_.load(mapPath);\n  if(map)map_.loadPolitics("assets/politics/political_1989.gzp");')
p.write_text(s)
print('Patched politics')
