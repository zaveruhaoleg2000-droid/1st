#include "gz/Campaign.hpp"
#include "gz/Europe.hpp"
#include "gz/Menu.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace gz;
static void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
int main(){try{
 ScenarioCatalog cat;check(cat.loadDirectory("assets/scenarios"),"Cannot parse scenario scripts");
 check(cat.all().size()==3,"There must be three standalone scenario scripts");
 check(cat.find("vistula_corridor")!=nullptr,"Wrong scenario id");
 MenuState menu;check(menu.activate(0)==MenuAction::None&&menu.page==MenuPage::Scenario,"New Game must show scenario selection");
 check(menu.activate(1)==MenuAction::StartScenario2&&menu.page==MenuPage::Playing,"Second scenario selection not wired");
 auto names=cat.all();check(names[0].id=="vistula_corridor"&&names[1].id=="prypiat_line"&&names[2].id=="danube_archive","Scenario ordering changed");
 for(const auto& scenario:cat.all()){
  check(scenario.events.size()==3 && scenario.initialFunds>0,"Scenario missing events/budget");
  Simulation game(4);game.newOperation(64);
  Campaign campaign;campaign.begin(scenario,game);
  check(campaign.state().funds==scenario.initialFunds,"Scenario budget wrong");
  check(!campaign.update(game,cat),"Events must not fire at tick 0");
  check(campaign.launchContract(game),"Mission deployment should succeed");
  game.step(220);
  check(campaign.update(game,cat),"Mission events should progress deterministically");
  check(campaign.state().completedEvents==7 && campaign.state().phase==1,"Missing event transition");
  check(!campaign.update(game,cat),"Events executed twice");
  const auto original=campaign.state();
  const auto path=std::filesystem::temp_directory_path()/("gz_campaign_"+scenario.id+".sav");
  check(campaign.saveSlot(path.string(),game),"Campaign atomic save failed");
  Simulation reloaded(4);reloaded.newOperation(16);Campaign restored;
  check(restored.loadSlot(path.string(),reloaded,cat),"Campaign load failed");
  check(reloaded.ticks()==game.ticks()&&restored.state().scenarioId==scenario.id,"Round trip state wrong");
  check(restored.state().funds==original.funds&&restored.state().completedEvents==original.completedEvents&&restored.state().reputation==original.reputation,"Lost money or reputation/events");
  const auto oldTick=reloaded.ticks(); const auto oldMoney=restored.state().funds;
  {
   std::fstream f(path,std::ios::binary|std::ios::in|std::ios::out);check(bool(f),"Cannot corrupt temp file");
   f.seekp(45,std::ios::beg);char byte=0x7f;f.write(&byte,1);
  }
  check(!restored.loadSlot(path.string(),reloaded,cat),"Corrupt campaign was accepted");
  check(reloaded.ticks()==oldTick && restored.state().funds==oldMoney,"Bad load destroyed active game");
  std::filesystem::remove(path);
 }
 EuropeBackdrop map;check(map.load("assets/europe/europe_vertices.gzm"),"Europe height mesh");
 check(map.loadPolitics("assets/politics/political_1989.gzp"),"Political layer load");
 check(map.politicalAt(21,52.2f)==6,"Poland political ID");
 check(map.politicalAt(30.5f,50.45f)==7,"Ukrainian SSR should be under Soviet polity index");
 check(map.politicalAt(27.56f,53.9f)==8,"Belarusian SSR political ID");
 check(map.politicalAt(19.05f,47.5f)==10,"Hungarian political ID");
 check(map.politicalAt(2.35f,48.86f)==1,"NATO political ID");
 check(map.politicalAt(-17.f,42.f)==0,"Ocean must not be political territory");
 std::vector<std::uint32_t> pixels;map.render(960,540,pixels,0,true,true);
 check(map.triangles()>50000,"Political map must remain actual vertex mesh");
 std::cout<<"Three scenario scripts, event progression, saved campaign checksum, and political pixel IDs passed\n";
 return 0;
}catch(const std::exception& e){std::cerr<<"Campaign test FAIL: "<<e.what()<<"\n";return 1;}}
