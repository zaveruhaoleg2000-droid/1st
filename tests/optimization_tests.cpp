#include "gz/Renderer.hpp"
#include "gz/Terrain.hpp"
#include "gz/Simulation.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>

static void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}

int main(){
  try {
    // The fast generator must remain bit-identical to the original public
    // continuous elevation function, even for negative sector coordinates.
    for(gz::TerrainSectorId id : {gz::TerrainSectorId{0,0},gz::TerrainSectorId{3,1},gz::TerrainSectorId{-2,-1}}){
      gz::HeightMap h;require(h.generate(id),"heightmap generation");
      for(int z=0;z<=gz::kTerrainCells;z+=37){
        for(int x=0;x<=gz::kTerrainCells;x+=43){
          const double wx=id.x*gz::kTerrainSectorSize+double(x)*gz::kTerrainSectorSize/gz::kTerrainCells;
          const double wz=id.z*gz::kTerrainSectorSize+double(z)*gz::kTerrainSectorSize/gz::kTerrainCells;
          const long dm=std::lround(double(gz::proceduralElevation(wx,wz))*10.);
          const auto original=std::int16_t(std::clamp(dm,-32768L,32767L));
          require(h.pixel(std::size_t(x),std::size_t(z))==original,"height pixel changed by optimization");
        }
      }
    }
    gz::TerrainWorld world(16);
    world.setTileDirectory("assets/terrain");
    // 20 sectors covering the initial location are packaged. Edited PGM still
    // has precedence over generated tile data.
    for(int x=-1;x<=3;++x){
      const double wx=double(x)*gz::kTerrainSectorSize+15;
      for(int z=-1;z<=2;++z){
        const double wz=double(z)*gz::kTerrainSectorSize+25;
        require(std::isfinite(world.heightAt(wx,wz)),"nonfinite terrain");
      }
    }
    require(world.sectorLoads()==0,"launch area should use prebaked tiles");
    require(world.diskLoads()==20,"missing launch tile assets");
    require(world.cachedSectorCount()<=16,"terrain cache is unbounded");
    require(world.cachedBytes()<=16u*gz::kTerrainSamples*gz::kTerrainSamples*2u,"terrain cache exceeds byte budget");

    gz::Simulation sim(4);sim.newOperation(96);
    gz::Renderer renderer;
    renderer.resize(640,400);
    renderer.setTerrainDirectory("assets/terrain");
    renderer.loadTextures("assets/textures/runtime",256);
    renderer.setVisualTime(7.f);
    gz::Camera cam;cam.focusX=3000;cam.focusZ=1250;cam.distance=1650;cam.pitch=.85f;
    renderer.draw(sim,cam,0);
    require(renderer.terrainLoads()==0,"launch camera still generates terrain");
    const auto tris=renderer.lastTriangles();
    require(tris>1000,"tactical scene missing geometry");
    renderer.draw(sim,cam,0);
    require(renderer.terrainLoads()==0 && renderer.lastTriangles()==tris,"repeated frame is not cached");
    std::cout<<"OPTIMIZATION PASS: reference heightmap parity, 20 baked tiles, bounded LRU, cached tactical frames\n";
    return 0;
  }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
