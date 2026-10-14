#include "gz/Textures.hpp"
#include "gz/Renderer.hpp"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>
int main(){
  gz::TextureLibrary lib;
  for(int r:{128,256,512}){
    if(lib.load("assets/textures/runtime",r)!=std::size_t(gz::TextureMaterial::Count)-1){std::cerr<<"Texture count failed at "<<r<<"\n";return 1;}
    if(!lib.has(gz::TextureMaterial::GrassShort)||!lib.has(gz::TextureMaterial::TreeBark))return 2;
    const auto a=lib.sample(gz::TextureMaterial::GrassShort, .13f,.91f);
    const auto b=lib.sample(gz::TextureMaterial::GrassShort,1.13f,-.09f);
    // Float addition produces tiny deltas; samples stay equivalent in interior.
    if(a!=b){std::cerr<<"Wrapped UV coordinates mismatch\n";return 3;}
    if(lib.bytes()>(std::size_t(gz::TextureMaterial::Count)-1)*512u*512u*4u)return 4;
  }
  gz::Simulation sim(4);sim.newOperation(48);
  gz::Renderer renderer;renderer.resize(480,320);
  if(renderer.loadTextures("assets/textures/runtime",256)!=std::size_t(gz::TextureMaterial::Count)-1)return 5;
  gz::Camera cam;cam.focusX=3000;cam.focusZ=1250;cam.distance=480;cam.pitch=.66f;
  renderer.setTexturing(false);renderer.draw(sim,cam);
  std::vector<std::uint32_t> old(renderer.data(),renderer.data()+renderer.width()*renderer.height());
  renderer.setTexturing(true);renderer.draw(sim,cam);
  std::size_t changed=0;
  for(std::size_t i=0;i<old.size();++i)if(old[i]!=renderer.data()[i])++changed;
  if(changed<200){std::cerr<<"Texture path is not visible: "<<changed<<" pixels\n";return 6;}
  std::cout<<"TEXTURES OK: 31 materials at 3 selectable runtime sizes, "
           <<renderer.textureMemoryBytes()/1024<<" KiB loaded, "<<changed<<" framebuffer pixels affected\n";
}
