#include "gz/GeometryStaging.hpp"
#include "gz/Diagnostics.hpp"
#include "gz/Europe.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>
int main(){
  using namespace gz;
  FrameMeter meter;
  meter.record(10,20);meter.record(30,40);
  auto a=meter.snapshot(12345,1024*1024,65536,"Software");
  assert(meter.sampleCount()==2);
  assert(std::abs(a.fps-33.3333f)<.1f&&std::abs(a.sceneMs-20)<.01f);
  for(int i=0;i<120;++i)meter.record(5,10);
  a=meter.snapshot(3,0,0,"Vulkan");
  assert(meter.sampleCount()==60&&std::abs(a.fps-100.f)<.01f);
  meter.record(-1,-1);assert(meter.sampleCount()==60);
  DebugHud hud;
  assert(hud.load("assets/ui/glyphs.pgm"));
  std::vector<std::uint32_t> picture(640*400,0xff324456);
  hud.draw(picture.data(),640,400,a);
  assert(picture[102*640+12]!=0xff324456); // panel actually rendered
  assert(!hud.load("missing.pgm"));
  EuropeBackdrop map;
  assert(map.load("assets/europe/europe_vertices.gzm"));
  assert(map.loadPolitics("assets/politics/political_1989.gzp"));
  map.setReliefScale(0.f);
  std::vector<std::uint32_t> frame;
  map.render(960,600,frame,0,true,true);
  // Look up a verified-land vertex by projecting its actual lat/lon.
  // Warsaw 21E / 52N is in Poland on the source political mask.
  const float u=(21.f-(-20.f))/80.f,v=(76.f-52.f)/49.f;
  const float x=960.f*(.015f+u*.955f+(v-.5f)*.032f);
  const float y=600.f*(.048f+v*.906f+u*.032f);
  assert(map.selectAt(int(x),int(y),15.f));
  const auto selected=map.selection();assert(selected.has_value());
  assert(std::abs(selected->longitude-21.f)<1.f);
  assert(std::abs(selected->latitude-52.f)<1.f);
  assert(selected->politicalZone!=0);
  assert(!map.selectAt(-20,20));assert(map.selection().has_value());
  map.setView(1.6f,.10f,-.05f);
  map.render(960,600,frame,0,true,true);
  const float zx=960.f*.64f+(x-960.f*.64f)*1.6f+960.f*.1f;
  const float zy=600.f*.50f+(y-600.f*.50f)*1.6f-600.f*.05f;
  assert(map.selectAt(int(zx),int(zy),15.f));
  assert(std::abs(map.selection()->longitude-21.f)<1.f);
  map.clearSelection();assert(!map.selection().has_value());
  struct Vertex {float x,y,z;};
  Vertex points[3]={{0,0,0},{1,0,0},{0,1,0}};
  std::uint32_t indices[3]={0,1,2};
  GeometryView geo{points,sizeof(points),sizeof(Vertex),indices,3};
  GeometryStagingCache cache(1024);
  assert(cache.stage(18,geo)==GeometryStagingCache::Result::Stored);
  const auto version=cache.revision(18);
  assert(cache.view(18).valid());
  assert(cache.stage(18,geo)==GeometryStagingCache::Result::Unchanged);
  assert(cache.revision(18)==version);
  points[1].x=2.f;
  assert(cache.stage(18,geo)==GeometryStagingCache::Result::Stored);
  assert(cache.revision(18)>version&&cache.changedUploads()==2);
  indices[2]=99;
  assert(cache.stage(18,geo)==GeometryStagingCache::Result::Invalid);
  assert(cache.revision(18)>version);
  indices[2]=2;
  GeometryStagingCache tiny(8);
  assert(tiny.stage(1,geo)==GeometryStagingCache::Result::OverBudget);
  assert(cache.erase(18));assert(cache.residentBytes()==0&&!cache.view(18).valid());
  std::cout<<"v0.0.14: map picking, debug HUD, frame meter and staged geometry PASS\n";
}
