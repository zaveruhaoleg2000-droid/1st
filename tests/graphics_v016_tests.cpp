#include "gz/LandscapeVisual.hpp"
#include "gz/Renderer.hpp"
#include "TestCheck.hpp"
#include <cmath>
#include <iostream>
#include <cstdint>
int main(){
  // Any adjacent heightmap chunks must share seamlessly varying visual material masks.
  for(float seam : {0.f, 187.5f, 3000.f, -187.5f, -3000.f}){
    auto a=gz::landscapeAt(seam-.001f,1190.f);
    auto b=gz::landscapeAt(seam+.001f,1190.f);
    GZ_CHECK(std::abs(a.soil-b.soil)<.002f);
    GZ_CHECK(std::abs(a.meadow-b.meadow)<.002f);
    GZ_CHECK(a.soil>=0 && a.soil<=1 && a.meadow>=0 && a.meadow<=1);
  }
  GZ_CHECK(std::abs(gz::roadCenterHorizontal(300.f)-gz::roadCenterHorizontal(300.001f))<.01f);
  GZ_CHECK(std::abs(gz::roadCenterVertical(1300.f)-gz::roadCenterVertical(1300.001f))<.01f);
  gz::Renderer renderer;
  renderer.resize(480,320);
  renderer.setTerrainDirectory("assets/terrain");
  GZ_CHECK(renderer.loadTextures("assets/textures/runtime",128)>9);
  gz::Simulation sim(2);sim.newOperation(48);sim.setMode(gz::ViewMode::Command);
  gz::Camera camera{};camera.focusX=3050;camera.focusZ=1250;camera.distance=440;camera.pitch=.6f;
  renderer.setVisualTime(10.f);renderer.setShaderQuality(gz::ShaderQuality::Balanced);
  renderer.draw(sim,camera,0);
  const auto triangles=renderer.lastTriangles();
  GZ_CHECK(triangles>500);
  const auto size=std::size_t(renderer.width())*renderer.height();
  std::vector<std::uint32_t> day(renderer.data(),renderer.data()+size);
  gz::AtmosphereSettings setting;setting.time=gz::TimeOfDay::Sunset;renderer.setAtmosphere(setting);
  renderer.draw(sim,camera,0);
  std::size_t changed=0;
  for(std::size_t i=0;i<size;i++)changed+=day[i]!=renderer.data()[i];
  GZ_CHECK(changed>size/3);
  // Debug overlay must not be permanently embedded into the normal game image.
  renderer.setAtmosphere({});renderer.setSectorGrid(false);renderer.draw(sim,camera,0);
  day.assign(renderer.data(),renderer.data()+size);
  renderer.setSectorGrid(true);renderer.draw(sim,camera,0);
  changed=0;
  for(std::size_t i=0;i<size;i++)changed+=day[i]!=renderer.data()[i];
  GZ_CHECK(changed>0);
  renderer.setSectorGrid(false);
  GZ_CHECK(!renderer.sectorGrid());
  std::cout<<"graphics v0.0.16: seamless masks, roads, sky, gables and optional sector grid OK\n";
}
