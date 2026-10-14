#include "gz/Menu.hpp"
#include <chrono>
#include <iostream>
#include <string>
int main(int argc,char** argv){
  gz::MenuScene scene;
  if(!scene.load("assets/europe/europe_vertices.gzm","assets/ui/glyphs.pgm","assets/ui/advances.bin")){
    std::cerr<<"Europe menu assets not found: run from project root\n";return 1;
  }
  gz::MenuState state;
  auto start=std::chrono::steady_clock::now();
  scene.draw(state,1136,640,4.f);
  double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
  const std::string out=argc>1?argv[1]:"preview_menu.ppm";
  if(!scene.savePPM(out))return 2;
  std::cout<<"Real menu framebuffer: "<<out<<" | "<<scene.terrainVertices()<<" vertices, "<<scene.terrainTriangles()<<" visible terrain triangles, "<<ms<<"ms\n";
  state.activate(2);state.activate(0);
  if(argc>2){scene.draw(state,1136,640,4.f);if(!scene.savePPM(argv[2]))return 3;}
  if(argc>3){state.back();state.activate(1);scene.draw(state,1136,640,4.f);if(!scene.savePPM(argv[3]))return 4;}
  return 0;
}
