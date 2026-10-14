from pathlib import Path
R=Path(__file__).resolve().parents[1]
p=R/'include/gz/Europe.hpp';s=p.read_text()
s=s.replace('    bool politicsLoaded() const', '    void setCampaignBanner(std::string news,int funds){news_=std::move(news);funds_=funds;}\n    bool politicsLoaded() const')
s=s.replace('    std::vector<std::uint8_t> political_,glyphs_;','    std::vector<std::uint8_t> political_,glyphs_;\n    std::string news_;int funds_=0;')
s=s.replace('#include <string_view>','#include <string_view>\n#include <utility>')
p.write_text(s)
p=R/'include/gz/Renderer.hpp';s=p.read_text()
s=s.replace('    void setPoliticalMap(bool on)', '    void setCampaignBanner(const std::string& news,int funds){europe_.setCampaignBanner(news,funds);}\n    void setPoliticalMap(bool on)')
p.write_text(s)
p=R/'src/Europe.cpp';s=p.read_text()
needle='  if(strategic&&political&&politicsLoaded()){\n    struct City'
# add UTF-8 decoder utility in anonymous namespace, inside second? global namespace `namespace {` ends near after shade before `}`; easiest add before glyphText function
insert='''static std::u32string utf8ToUnicode(const std::string& bytes){
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
'''
s=s.replace('void EuropeBackdrop::glyphText(',insert+'\nvoid EuropeBackdrop::glyphText(')
needle='''    fill(9,h_-57,321,49,rgb(8,17,23),.77f);'''
replace='''    if(!news_.empty()){
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
    fill(9,h_-57,321,49,rgb(8,17,23),.77f);'''
assert needle in s
s=s.replace(needle,replace)
p.write_text(s)
p=R/'src/X11App.cpp';s=p.read_text()
s=s.replace('''            render.draw(sim,camera,int(selected));''','''            if(campaign.active())render.setCampaignBanner(campaign.state().latestNews,campaign.state().funds);
            render.draw(sim,camera,int(selected));''')
p.write_text(s)
p=R/'src/Win32App.cpp';s=p.read_text()
s=s.replace('''        if(showMenu)menuScene.draw(menu,renderer.width(),renderer.height(),renderer.visualTime());
        else renderer.draw(sim,camera,selected);''','''        if(showMenu)menuScene.draw(menu,renderer.width(),renderer.height(),renderer.visualTime());
        else{
            if(campaign.active())renderer.setCampaignBanner(campaign.state().latestNews,campaign.state().funds);
            renderer.draw(sim,camera,selected);
        }''')
p.write_text(s)
p=R/'src/Menu.cpp';s=p.read_text().replace('GRAY ZONE  /  ТЕСТОВИЙ БІЛД 01','GRAY ZONE  /  ТЕСТОВИЙ БІЛД 02')
p.write_text(s)
print('HUD patched')
