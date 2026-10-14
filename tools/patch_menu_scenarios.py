from pathlib import Path
R=Path(__file__).resolve().parents[1]
p=R/'include/gz/Menu.hpp';s=p.read_text()
s=s.replace('enum class MenuPage {Main, Load, Settings, Playing};','enum class MenuPage {Main, Load, Settings, Scenario, Playing};')
s=s.replace('enum class MenuAction {None, NewGame, LoadSlot1, LoadSlot2, LoadSlot3, Exit, SettingsChanged};','enum class MenuAction {None, NewGame, LoadSlot1, LoadSlot2, LoadSlot3, Exit, SettingsChanged, StartScenario1, StartScenario2, StartScenario3};')
s=s.replace('    std::vector<std::uint8_t> glyphs_,advances_;','''    std::vector<std::uint8_t> glyphs_,advances_;
    std::vector<std::uint8_t> flags_,portraits_;
    void atlasTile(const std::vector<std::uint8_t>& source,int tileW,int tileH,int index,int x,int y,int zoom);''')
p.write_text(s)
p=R/'src/Menu.cpp';s=p.read_text()
s=s.replace('void MenuState::back(){if(page==MenuPage::Load||page==MenuPage::Settings){','void MenuState::back(){if(page==MenuPage::Load||page==MenuPage::Settings||page==MenuPage::Scenario){')
s=s.replace('  case MenuPage::Playing:return {};','''  case MenuPage::Scenario:{static const std::u32string labels[]={U"Варшавський коридор",U"Лінія Прип'яті",U"Дунайський архів",U"Назад"};return labels[i];}
  case MenuPage::Playing:return {};''')
s=s.replace('    if(i==0){page=MenuPage::Playing;return MenuAction::NewGame;}','    if(i==0){page=MenuPage::Scenario;selected=0;return MenuAction::None;}')
s=s.replace('  if(i==3){back();return MenuAction::None;}','''  if(i==3){back();return MenuAction::None;}
  if(page==MenuPage::Scenario){page=MenuPage::Playing;return MenuAction(int(MenuAction::StartScenario1)+i);}''')
s=s.replace('  return map && !glyphs_.empty() && advances_.size()==sizeof(kMenuGlyphs)/sizeof(char32_t)-1;','''  auto loadAtlas=[](const char* filename,std::size_t bytes,std::vector<std::uint8_t>& target){
    std::ifstream f(filename,std::ios::binary);
    if(!f)return;
    target.assign(std::istreambuf_iterator<char>(f),{});
    if(target.size()!=bytes)target.clear();
  };
  loadAtlas("assets/ui/pmc_flags.rgb",6*112*4*80*3,flags_);
  loadAtlas("assets/ui/pmc_portraits.rgb",6*88*4*96*3,portraits_);
  return map && !glyphs_.empty() && advances_.size()==sizeof(kMenuGlyphs)/sizeof(char32_t)-1;''')
needle='void MenuScene::rect(int x,int y,int width,int height,std::uint32_t c,float alpha){'
add='''void MenuScene::atlasTile(const std::vector<std::uint8_t>& source,int tw,int th,int index,int x,int y,int zoom){
  if(source.empty()||index<0||index>=24||zoom<=0)return;
  const int fullW=tw*6,ox=(index%6)*tw,oy=(index/6)*th;
  for(int yy=0;yy<th*zoom;++yy){int py=y+yy;if(py<0||py>=h_)continue;
    for(int xx=0;xx<tw*zoom;++xx){int px=x+xx;if(px<0||px>=w_)continue;
      const std::size_t i=(std::size_t(oy+yy/zoom)*fullW+ox+xx/zoom)*3;
      buffer_[std::size_t(py)*w_+px]=rgb(source[i],source[i+1],source[i+2]);
    }
  }
}
'''
s=s.replace(needle,add+needle)
s=s.replace('  const auto mapLabel=menu.page==MenuPage::Main?U"ЄВРОПА  /  1989":menu.page==MenuPage::Load?U"ЗБЕРЕЖЕННЯ ОПЕРАЦІЇ":U"НАЛАШТУВАННЯ РУШІЯ";', '  const auto mapLabel=menu.page==MenuPage::Main?U"ЄВРОПА  /  1989":menu.page==MenuPage::Load?U"ЗБЕРЕЖЕННЯ ОПЕРАЦІЇ":menu.page==MenuPage::Scenario?U"ПОЧАТОК КАМПАНІЇ":U"НАЛАШТУВАННЯ РУШІЯ";')
s=s.replace('  else if(menu.page==MenuPage::Settings)text(', '  else if(menu.page==MenuPage::Scenario)text(int(w_*.036f),int(h_*.80f),U"3 кампанії  /  8 фракцій  /  24 ЧВК",.46f*s,muted);\n  else if(menu.page==MenuPage::Settings)text(')
s=s.replace('  text(int(w_*.757f),int(h_*.935f),U"СТРАТЕГІЧНА МАПА",.46f*s,muted);','''  if(menu.page==MenuPage::Scenario){
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
  text(int(w_*.757f),int(h_*.935f),menu.page==MenuPage::Scenario?U"ВИБІР СЦЕНАРІЮ":U"СТРАТЕГІЧНА МАПА",.46f*s,muted);''')
p.write_text(s)
print('Patched menu scenarios')
