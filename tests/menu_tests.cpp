#include "gz/Menu.hpp"
#include "gz/Simulation.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
namespace {
void require(bool ok,const char* desc){if(!ok)throw std::runtime_error(desc);}
}
int main(){try{
  gz::MenuState st;
  require(st.rows()==4 && st.page==gz::MenuPage::Main,"Main menu rows");
  st.move(-1);require(st.selected==3,"Wrap keys");st.move(1);
  require(st.activate(1)==gz::MenuAction::None && st.page==gz::MenuPage::Load,"Load page");
  require(st.activate(0)==gz::MenuAction::None && st.page==gz::MenuPage::Load,"Empty slot blocked");
  st.saveExists[0]=true;
  require(st.activate(0)==gz::MenuAction::LoadSlot1 && st.page==gz::MenuPage::Playing,"Load slot action");
  st.page=gz::MenuPage::Main;
  st.activate(2);require(st.page==gz::MenuPage::Settings,"Settings navigation");
  require(st.aa && st.wind && st.water,"Initial effects");
  require(st.activate(0)==gz::MenuAction::SettingsChanged && !st.aa,"AA toggle");
  require(st.activate(1)==gz::MenuAction::SettingsChanged && !st.wind,"Wind toggle");
  require(st.activate(2)==gz::MenuAction::SettingsChanged && !st.water,"Water toggle");
  st.back();require(st.page==gz::MenuPage::Main,"Settings return");
  require(gz::MenuState::hitRow(75,251,1136,640)==0,"Menu mouse hover");
  require(gz::MenuState::hitRow(1000,251,1136,640)==-1,"Ignore outside mouse");
  gz::MenuScene scene;
  require(scene.load("assets/europe/europe_vertices.gzm","assets/ui/glyphs.pgm","assets/ui/advances.bin"),"Load map/fonts");
  require(scene.terrainVertices()==257u*153u,"Each pixel is a vertex");
  auto before=std::filesystem::temp_directory_path()/"grayzone_menu_test.ppm";
  scene.draw(st,640,360,0.f);require(scene.terrainTriangles()>5000,"3D mesh rasterizes triangles");
  require(scene.savePPM(before.string()),"Menu render screenshot");
  require(std::filesystem::file_size(before)>640u*360u*3u,"Rendered screenshot bytes");
  std::filesystem::remove(before);
  gz::Simulation sim(4);sim.newOperation(36);
  auto save=std::filesystem::temp_directory_path()/"grayzone_menu_save_test.sav";
  require(sim.save(save.string()),"Save operation");
  gz::Simulation fresh(4);require(fresh.load(save.string()),"Load saved operation");
  require(fresh.squadCount()==sim.squadCount(),"Saved squads restored");
  std::filesystem::remove(save);
  std::cout<<"Europe 3D terrain + menu controls + settings + save/load OK\n";
  return 0;
}catch(const std::exception& e){std::cerr<<"Menu test failed: "<<e.what()<<"\n";return 1;}}
