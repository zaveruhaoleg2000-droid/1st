#include "gz/Europe.hpp"
#include "gz/Glyphs.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
namespace gz {
namespace {
static constexpr std::array<std::uint32_t,16> kPolitics={
  0x112838u,0x386985u,0x794e55u,0x763939u,0x747f6au,0x766d51u,0xb68a50u,0x89644bu,
  0x765758u,0x705c81u,0x9b6c65u,0x8d4d58u,0x457a9au,0x86627fu,0x795866u,0x85545eu};
std::uint32_t rgb(unsigned r,unsigned g,unsigned b){return ((r&255u)<<16)|((g&255u)<<8)|(b&255u);}
std::uint32_t shade(std::uint32_t c,float v){v=std::clamp(v,0.f,1.6f);return rgb(unsigned(std::min(255.f,float((c>>16)&255u)*v)),unsigned(std::min(255.f,float((c>>8)&255u)*v)),unsigned(std::min(255.f,float(c&255u)*v)));}
}
std::uint32_t EuropeBackdrop::mix(std::uint32_t a,std::uint32_t b,float t)const{
  t=std::clamp(t,0.f,1.f);
  return rgb(unsigned(((a>>16)&255u)*(1-t)+((b>>16)&255u)*t),unsigned(((a>>8)&255u)*(1-t)+((b>>8)&255u)*t),unsigned((a&255u)*(1-t)+(b&255u)*t));
}
void EuropeBackdrop::setView(float zoom,float panX,float panY){
  if(!std::isfinite(zoom)||!std::isfinite(panX)||!std::isfinite(panY))return;
  viewZoom_=std::clamp(zoom,.7f,3.f);panX_=std::clamp(panX,-.8f,.8f);panY_=std::clamp(panY,-.8f,.8f);
}
void EuropeBackdrop::setReliefScale(float scale){
  if(std::isfinite(scale))reliefScale_=std::clamp(scale,0.f,.035f);
}
bool EuropeBackdrop::load(const std::string& path){
  std::ifstream in(path,std::ios::binary);if(!in)return false;
  char magic[5]{};in.read(magic,5);
  if(std::string(magic,5)!="GZEU1")return false;
  auto u16=[&](){unsigned char a=0,b=0;in.read(reinterpret_cast<char*>(&a),1);in.read(reinterpret_cast<char*>(&b),1);return int(a)|(int(b)<<8);};
  int w=u16(),h=u16();if(w<3||w>2000||h<3||h>2000)return false;
  std::array<float,4> coords{};in.read(reinterpret_cast<char*>(coords.data()),int(coords.size()*sizeof(float)));
  if(!in || !(coords[0]<coords[1]&&coords[2]>coords[3]))return false;
  std::vector<Vertex> tmp(std::size_t(w)*h);
  for(auto& v:tmp){
    v.altitude=std::uint16_t(u16());
    unsigned char c[4]{};in.read(reinterpret_cast<char*>(c),4);
    v.r=c[0];v.g=c[1];v.b=c[2];v.land=c[3];
  }
  if(!in)return false;
  sourceW_=w;sourceH_=h;west_=coords[0];east_=coords[1];north_=coords[2];south_=coords[3];
  vertices_=std::move(tmp);projected_.resize(vertices_.size());selected_.reset();return true;
}

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
  std::ifstream advances("assets/ui/advances.bin",std::ios::binary);advances_.assign(std::istreambuf_iterator<char>(advances),{});
  return true;
}
std::uint8_t EuropeBackdrop::politicalAt(float lon,float lat)const{
  if(!politicsLoaded()||lon<west_||lon>east_||lat<south_||lat>north_)return 0;
  int x=std::clamp(int(std::round((lon-west_)/(east_-west_)*(sourceW_-1))),0,sourceW_-1);
  int y=std::clamp(int(std::round((north_-lat)/(north_-south_)*(sourceH_-1))),0,sourceH_-1);
  return political_[std::size_t(y)*sourceW_+x];
}
bool EuropeBackdrop::selectAt(int screenX,int screenY,float radiusPixels){
  if(vertices_.empty()||projected_.size()!=vertices_.size()||w_<=0||h_<=0||
     screenX<0||screenY<0||screenX>=w_||screenY>=h_||
     !std::isfinite(radiusPixels)||radiusPixels<=0.f)return false;
  float best=radiusPixels*radiusPixels;
  std::size_t bestIndex=vertices_.size();
  for(std::size_t i=0;i<vertices_.size();++i){
    if(!vertices_[i].land)continue;
    const auto& q=projected_[i];
    const float dx=q.x-float(screenX),dy=q.y-float(screenY);
    const float d2=dx*dx+dy*dy;
    if(d2<best){best=d2;bestIndex=i;}
  }
  if(bestIndex==vertices_.size())return false; // Keep previous selection until a valid click.
  const int gx=int(bestIndex%std::size_t(sourceW_)),gy=int(bestIndex/std::size_t(sourceW_));
  selected_=MapSelection{west_+float(gx)/float(sourceW_-1)*(east_-west_),
      north_-float(gy)/float(sourceH_-1)*(north_-south_),vertices_[bestIndex].altitude,
      political_.size()==vertices_.size()?political_[bestIndex]:std::uint8_t(0),gx,gy};
  return true;
}
static std::u32string asciiToU32(const std::string& text){
  std::u32string result;result.reserve(text.size());
  for(unsigned char c:text)result.push_back(char32_t(c));
  return result;
}
static std::u32string utf8ToUnicode(const std::string& bytes){
  std::u32string out;
  for(std::size_t i=0;i<bytes.size();){
    const unsigned char a=std::uint8_t(bytes[i++]);
    char32_t c=0;int trail=0;
    if(a<128)c=a;
    else if((a&0xe0)==0xc0){c=a&0x1f;trail=1;}
    else if((a&0xf0)==0xe0){c=a&0x0f;trail=2;}
    else if((a&0xf8)==0xf0){c=a&7;trail=3;}
    else continue;
    bool valid=true;for(int j=0;j<trail;++j){if(i>=bytes.size()||((unsigned char)bytes[i]&0xc0)!=0x80){valid=false;break;}c=(c<<6)|(bytes[i++]&0x3f);}
    if(valid&&c>=32&&c<=0x10ffff)out.push_back(c);
  }
  return out;
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
    cursor+=std::max(3,int((idx<advances_.size()?advances_[idx]:24)*scale));
  }
}

void EuropeBackdrop::point(int x,int y,std::uint32_t c){if(x>=0 && y>=0 && x<w_ && y<h_)(*pixels_)[std::size_t(y)*w_+x]=c;}
void EuropeBackdrop::fill(int x,int y,int w,int h,std::uint32_t c,float alpha){
  int x0=std::clamp(x,0,w_),x1=std::clamp(x+w,0,w_),y0=std::clamp(y,0,h_),y1=std::clamp(y+h,0,h_);
  for(int yy=y0;yy<y1;++yy)for(int xx=x0;xx<x1;++xx){auto& p=(*pixels_)[std::size_t(yy)*w_+xx];p=mix(p,c,alpha);}
}
void EuropeBackdrop::line(float x0,float y0,float x1,float y1,std::uint32_t c,float alpha,int thickness){
  const float dx=x1-x0,dy=y1-y0;
  const int steps=int(std::ceil(std::max(std::abs(dx),std::abs(dy))));if(steps<1||steps>10000)return;
  for(int i=0;i<=steps;++i){int x=int(std::round(x0+dx*float(i)/steps)),y=int(std::round(y0+dy*float(i)/steps));
    for(int a=0;a<thickness;++a)for(int b=0;b<thickness;++b){int px=x+a,py=y+b;
      if(px>=0&&py>=0&&px<w_&&py<h_){auto& p=(*pixels_)[std::size_t(py)*w_+px];p=mix(p,c,alpha);}
    }
  }
}
void EuropeBackdrop::triangle(const Projected& a,const Projected& b,const Projected& c,std::uint32_t& dummy){
  // 3D vertex mesh: both projected xy AND triangle shade depend on individual heightmap pixels.
  (void)dummy;
  float minx=std::min({a.x,b.x,c.x}),maxx=std::max({a.x,b.x,c.x});
  float miny=std::min({a.y,b.y,c.y}),maxy=std::max({a.y,b.y,c.y});
  if(maxx<0||maxy<0||minx>=w_||miny>=h_)return;
  int x0=std::max(0,int(minx)),y0=std::max(0,int(miny)),x1=std::min(w_-1,int(std::ceil(maxx))),y1=std::min(h_-1,int(std::ceil(maxy)));
  float den=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
  if(std::abs(den)<.0001f)return;
  float inv=1.f/den;
  // Directional sunlight on actual displaced triangular face (visual relief is deliberately exaggerated).
  float dsx=(b.height-a.height),dsy=(c.height-a.height);
  float relative=reliefScale_/.027f;
  float light=std::clamp(.98f-.00018f*dsx*relative+.00022f*dsy*relative,.80f,1.08f);
  auto color=mix(mix(a.color,b.color,.5f),c.color,.33333f);
  color=shade(color,light);(void)color;
  for(int y=y0;y<=y1;++y){float py=y+.5f;
    for(int x=x0;x<=x1;++x){float px=x+.5f;
      float wb=((px-a.x)*(c.y-a.y)-(py-a.y)*(c.x-a.x))*inv;
      float wc=((b.x-a.x)*(py-a.y)-(b.y-a.y)*(px-a.x))*inv;
      float wa=1-wb-wc;
      if(wa<-.001f||wb<-.001f||wc<-.001f)continue;
      unsigned channels[3]{};for(int k=0;k<3;++k){int shift=(2-k)*8;channels[k]=unsigned(std::clamp((((a.color>>shift)&255)*wa+((b.color>>shift)&255)*wb+((c.color>>shift)&255)*wc)*light,0.f,255.f));}
      (*pixels_)[std::size_t(y)*w_+x]=(channels[0]<<16)|(channels[1]<<8)|channels[2];
    }
  }
  ++drawnTriangles_;
}
void EuropeBackdrop::render(int width,int height,std::vector<std::uint32_t>& output,float seconds,bool strategic,bool political){
  w_=std::clamp(width,320,1920);h_=std::clamp(height,240,1080);
  pixels_=&output;output.resize(std::size_t(w_)*h_);drawnTriangles_=0;maxLiftPixels_=0.f;
  for(int y=0;y<h_;++y){auto c=mix(rgb(12,27,37),rgb(23,37,42),float(y)/h_);
    std::fill_n(output.begin()+std::size_t(y)*w_,w_,c);
  }
  if(vertices_.empty())return;
  const float drift=strategic?0.f:std::sin(seconds*.10f)*float(w_)*.006f;
  for(int y=0;y<sourceH_;++y){float v=float(y)/(sourceH_-1);
    for(int x=0;x<sourceW_;++x){float u=float(x)/(sourceW_-1);
      const auto& s=vertices_[std::size_t(y)*sourceW_+x];auto& p=projected_[std::size_t(y)*sourceW_+x];
      p.height=s.land?float(s.altitude):0.f;p.land=s.land!=0;
      p.x=w_*((strategic?.015f:.255f)+u*(strategic?.955f:.88f)+(v-.5f)*.032f)+drift;
      const float lift=p.height*(h_/650.f)*reliefScale_;
      maxLiftPixels_=std::max(maxLiftPixels_,lift);
      p.y=h_*(.048f+v*.906f+u*.032f)-lift;
      p.x=w_*.64f+(p.x-w_*.64f)*viewZoom_+panX_*w_;
      p.y=h_*.50f+(p.y-h_*.50f)*viewZoom_+panY_*h_;
      p.color=rgb(s.r,s.g,s.b);
      if(s.land){
        const auto sample=[&](int xx,int yy){return float(vertices_[std::size_t(std::clamp(yy,0,sourceH_-1))*sourceW_+std::clamp(xx,0,sourceW_-1)].altitude);};
        const float dx=sample(x+1,y)-sample(x-1,y),dy=sample(x,y+1)-sample(x,y-1);
        const float reliefLight=std::clamp(1.f-dx*.00022f+dy*.00028f,.58f,1.24f);
        const float forest=std::clamp((s.g-s.r)*.012f,0.f,1.f);
        p.color=mix(p.color,rgb(71,89,62),forest*.35f);
        p.color=shade(p.color,reliefLight);
        if(s.altitude>1800)p.color=mix(p.color,rgb(206,210,197),std::clamp((s.altitude-1800)/2200.f,0.f,.72f));
      }else p.color=mix(rgb(22,48,66),rgb(47,85,100),std::clamp(float(s.b)/255.f,0.f,1.f));
      if(political && politicsLoaded() && s.land){
        const std::uint8_t faction=political_[std::size_t(y)*sourceW_+x];
        if(faction)p.color=mix(p.color,kPolitics[faction],strategic?.16f:.08f);
      }
      if(s.land && u>.55f) p.color=mix(p.color,rgb(121,49,51),.025f*(u-.55f)/.45f); // geopolitical color grading, not borders
    }
  }
  std::uint32_t dummy=0;
  for(int y=0;y<sourceH_-1;++y){for(int x=0;x<sourceW_-1;++x){
    auto& a=projected_[std::size_t(y)*sourceW_+x];auto& b=projected_[std::size_t(y)*sourceW_+x+1];
    auto& c=projected_[std::size_t(y+1)*sourceW_+x];auto& d=projected_[std::size_t(y+1)*sourceW_+x+1];
    triangle(a,b,d,dummy);triangle(a,d,c,dummy);
  }}
  // Politically relevant borders are read from the same geo-registered image as the mesh.
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
  // Quiet military grid; visually separates terrain from the UI while preserving readability.
  for(int gx=1;gx<10;++gx){int ix=gx*(sourceW_-1)/10;
    for(int y=0;y<sourceH_-2;y+=4){const auto& a=projected_[std::size_t(y)*sourceW_+ix];const auto& b=projected_[std::size_t(y+2)*sourceW_+ix];
      line(a.x,a.y,b.x,b.y,rgb(180,204,204),.12f);
    }}
  for(int gy=1;gy<7;++gy){int iy=gy*(sourceH_-1)/7;
    for(int x=0;x<sourceW_-2;x+=4){const auto& a=projected_[std::size_t(iy)*sourceW_+x];const auto& b=projected_[std::size_t(iy)*sourceW_+x+2];
      line(a.x,a.y,b.x,b.y,rgb(180,204,204),.12f);
    }}
  // Strategic markers are representative UI locations, not real battlefield positions.
  auto marker=[&](float lon,float lat,bool enemy){
    float u=(lon-west_)/(east_-west_),v=(north_-lat)/(north_-south_);
    if(u<0||u>1||v<0||v>1)return;
    int x=std::clamp(int(u*(sourceW_-1)),0,sourceW_-1),y=std::clamp(int(v*(sourceH_-1)),0,sourceH_-1);
    auto p=projected_[std::size_t(y)*sourceW_+x];
    auto c=enemy?rgb(207,97,87):rgb(91,180,211);
    line(p.x-6,p.y,p.x+6,p.y,c,.95f,1);line(p.x,p.y-6,p.x,p.y+6,c,.95f,1);
    line(p.x-4,p.y-4,p.x+4,p.y-4,c,.7f);line(p.x-4,p.y+4,p.x+4,p.y+4,c,.7f);
  };
  marker(14.4f,50.1f,false);marker(21.f,52.f,false);marker(24.f,49.8f,true);marker(27.6f,53.9f,true);marker(30.5f,50.5f,true);
  if(strategic&&political&&politicsLoaded()){
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
    if(!news_.empty()){
      // This is deliberately in the actual global-map frame buffer, not a mockup.
      const int panelW=std::min(w_-24,700);
      fill(10,12,panelW,68,rgb(6,13,20),.79f);
      fill(10,12,4,68,rgb(189,148,87),.96f);
      auto decoded=utf8ToUnicode(news_);
      std::u32string line1,line2;
      std::size_t split=decoded.find(U':');
      if(split!=std::u32string::npos && split<65){line1=decoded.substr(0,split);line2=decoded.substr(split+1);}
      else{line1=U"ОПЕРАЦІЯ / БЮЛЕТЕНЬ";line2=decoded;}
      if(line1.size()>57)line1.resize(57);
      if(line2.size()>76)line2.resize(76);
      glyphText(26,19,line1,.39f*std::clamp(w_/960.f,.85f,1.2f),rgb(241,222,161));
      glyphText(26,46,line2,.30f*std::clamp(w_/960.f,.85f,1.2f),rgb(219,222,215));
      // Tiny funds bar serves as a useful gameplay cue without a large HUD panel.
      fill(panelW-110,18,88,5,rgb(60,77,70),.8f);
      fill(panelW-110,18,std::clamp(funds_/1000,0,88),5,rgb(181,153,81),.9f);
    }
    fill(9,h_-57,321,49,rgb(8,17,23),.77f);
    glyphText(18,h_-51,U"ЄВРОПА / БЕРЕЗЕНЬ 1989",.36f*std::clamp(w_/960.f,.75f,1.5f),rgb(235,223,192));
    fill(17,h_-23,17,7,kPolitics[1]);glyphText(39,h_-27,U"НАТО",.26f,rgb(221,230,225));
    fill(104,h_-23,17,7,kPolitics[3]);glyphText(126,h_-27,U"СРСР",.26f,rgb(221,230,225));
    fill(194,h_-23,17,7,kPolitics[6]);glyphText(216,h_-27,U"ПОЛЬЩА",.26f,rgb(221,230,225));
  }
  if(strategic&&selected_){
    const auto& s=*selected_;
    const auto& pointOnMap=projected_[std::size_t(s.gridY)*sourceW_+s.gridX];
    const auto gold=rgb(251,214,117);
    line(pointOnMap.x-11,pointOnMap.y,pointOnMap.x+11,pointOnMap.y,gold,.95f,2);
    line(pointOnMap.x,pointOnMap.y-11,pointOnMap.x,pointOnMap.y+11,gold,.95f,2);
    const int panelX=std::max(8,w_-264),panelY=std::max(86,h_/5);
    fill(panelX,panelY,std::min(256,w_-panelX-5),82,rgb(8,19,27),.87f);
    fill(panelX,panelY,3,82,gold,.95f);
    glyphText(panelX+10,panelY+7,U"ОБРАНА ТОЧКА",.31f,gold);
    const int lonTenths=int(std::round(std::abs(s.longitude)*10.f));
    const int latTenths=int(std::round(std::abs(s.latitude)*10.f));
    auto angle=[&](int tenths,bool positive,const char* plus,const char* minus){
      return std::to_string(tenths/10)+"."+std::to_string(tenths%10)+(positive?plus:minus);
    };
    const auto coords=angle(latTenths,s.latitude>=0,"N","S")+"  "+angle(lonTenths,s.longitude>=0,"E","W");
    glyphText(panelX+10,panelY+30,asciiToU32(coords),.28f,rgb(231,235,220));
    glyphText(panelX+10,panelY+55,U"ЗОНА "+asciiToU32(std::to_string(s.politicalZone))+
        U" / "+asciiToU32(std::to_string(s.altitudeMeters))+U" М",.26f,rgb(203,216,206));
  }
  if(strategic){
    // On the strategic view make the operational area visible; the battle renderer is not reused as a fake world.
    float x=w_*.58f,y=h_*.42f;
    line(x-17,y,x+17,y,rgb(238,204,126),.8f,2);
    line(x,y-16,x,y+16,rgb(238,204,126),.8f,2);
  }
  // Atmospheric fade to dark blue, particularly on the left where the menu will sit.
  for(int x=0;x<w_;++x){float fade=std::clamp((.31f-float(x)/w_)/.31f,0.f,1.f)*.82f;
    if(strategic || fade<=.01f)continue;
    for(int y=0;y<h_;++y){auto& p=output[std::size_t(y)*w_+x];p=mix(p,rgb(9,19,25),fade);}
  }
}
}
