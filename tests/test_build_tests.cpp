#include "gz/Europe.hpp"
#include "gz/Simulation.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

static void check(bool cond,const char* error){if(!cond)throw std::runtime_error(error);}
int main(){
 try{
  gz::EuropeBackdrop europe;
  check(europe.load("assets/europe/europe_vertices.gzm"),"Europe mesh cannot load");
  check(europe.vertexCount()==257u*153u,"Europe source dimensions wrong");
  std::vector<std::uint32_t> pixels;
  europe.render(960,540,pixels,0.f,true);
  const float balanced=europe.maximumLiftPixels();
  check(balanced>10.f && balanced<35.f,"Satellite map relief must be visible and bounded");
  check(europe.triangles()>50000,"Mesh triangles not rendered");
  europe.setReliefScale(.027f);europe.render(960,540,pixels,0.f,true);
  const float old=europe.maximumLiftPixels();
  check(old>balanced*1.3f,"Increasing relief must increase elevation");
  europe.setReliefScale(0.f);europe.render(960,540,pixels,0.f,true);
  check(europe.maximumLiftPixels()==0.f,"2D map relief setting must be perfectly flat");
  europe.setReliefScale(-5.f);
  check(europe.reliefScale()==0.f,"Negative relief not clamped");
  europe.setReliefScale(100.f);
  check(europe.reliefScale()<=.035f,"Too large relief not clamped");
  europe.setReliefScale(.006f);
  const auto temp=std::filesystem::temp_directory_path();
  gz::Simulation game(4);game.newOperation(64);game.step(32);
  const auto tick=game.ticks(),count=game.squadCount();
  const auto path=temp/"gz_test_build_valid.sav";
  const auto broken=temp/"gz_test_build_corrupt.sav";
  check(game.save(path.string()),"Saving healthy game failed");
  check(game.load(path.string()),"Reloading healthy game failed");
  check(game.ticks()==tick,"Ticks changed on round trip");
  {
   std::ifstream in(path,std::ios::binary);
   std::vector<char> data(std::istreambuf_iterator<char>{in},{});
   check(data.size()>48,"Save test asset too short");
   // Layout v2: magic[4], version[4], tick/shots/casualties[24], squads[4], soldiers[4].
   for(int n=32;n<40;++n)data[std::size_t(n)]=0;
   std::ofstream out(broken,std::ios::binary);out.write(data.data(),std::streamsize(data.size()));
  }
  check(!game.load(broken.string()),"Empty but well-formed save must be rejected");
  check(game.ticks()==tick && game.squadCount()==count,"Failed load modified ongoing game");
  {
   std::ofstream out(broken,std::ios::binary|std::ios::trunc);out<<"GZ89";
  }
  check(!game.load(broken.string()),"Truncated save must be rejected");
  check(game.squadCount()==count,"Truncated save changed gameplay state");
  std::filesystem::remove(path);std::filesystem::remove(broken);
  std::cout<<"Test-build regressions passed: 3D Europe relief "<<balanced<<"px versus "<<old
           <<"px; flat=0; save roundtrip and corrupt/truncated save handling.\n";
  return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
