#include "gz/Europe.hpp"
#include "gz/Menu.hpp"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
static bool ppm(const std::string& path,const std::vector<std::uint32_t>& c,int w,int h){
 std::ofstream f(path,std::ios::binary);if(!f)return false;
 f<<"P6\n"<<w<<' '<<h<<"\n255\n";
 for(auto x:c){char rgb[3]={char(x>>16),char(x>>8),char(x)};f.write(rgb,3);}return bool(f);
}
int main(int argc,char**argv){
 gz::EuropeBackdrop map;
 if(!map.load("assets/europe/europe_vertices.gzm")||!map.loadPolitics("assets/politics/political_1989.gzp")){
  std::cerr<<"Cannot load political layer\n";return 1;
 }
 std::vector<std::uint32_t> image;map.setReliefScale(.005f);
 map.render(1280,720,image,0,true,true);
 if(!ppm(argc>1?argv[1]:"political_preview.ppm",image,1280,720))return 2;
 gz::MenuScene menu;if(!menu.load("assets/europe/europe_vertices.gzm","assets/ui/glyphs.pgm","assets/ui/advances.bin"))return 3;
 gz::MenuState st;st.activate(0);st.selected=1;
 menu.draw(st,1280,720,0);
 if(!menu.savePPM(argc>2?argv[2]:"scenario_preview.ppm"))return 4;
 std::cout<<"Strategic politics+story menu generated: "<<map.vertexCount()<<" vertices\n";
 return 0;
}
