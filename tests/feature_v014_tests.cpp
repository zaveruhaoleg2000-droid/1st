#include "gz/GeometryStaging.hpp"
#include "gz/Diagnostics.hpp"
#include "gz/Europe.hpp"
#include "TestCheck.hpp"
#include <cmath>
#include <iostream>
#include <vector>
int main(){
  using namespace gz;
  FrameMeter meter;
  meter.record(10,20);meter.record(30,40);
  auto a=meter.snapshot(12345,1024*1024,65536,"Software");
  GZ_CHECK(meter.sampleCount()==2);
  GZ_CHECK(std::abs(a.fps-33.3333f)<.1f&&std::abs(a.sceneMs-20)<.01f);
  for(int i=0;i<120;++i)meter.record(5,10);
  a=meter.snapshot(3,0,0,"Vulkan");
  GZ_CHECK(meter.sampleCount()==60&&std::abs(a.fps-100.f)<.01f);
  meter.record(-1,-1);GZ_CHECK(meter.sampleCount()==60);
  DebugHud hud;
  GZ_CHECK(hud.load("assets/ui/glyphs.pgm"));
  std::vector<std::uint32_t> picture(640*400,0xff324456);
  hud.draw(picture.data(),640,400,a);
  GZ_CHECK(picture[102*640+12]!=0xff324456); // panel actually rendered
  GZ_CHECK(!hud.load("missing.pgm"));
  EuropeBackdrop map;
  GZ_CHECK(map.load("assets/europe/europe_vertices.gzm"));
  GZ_CHECK(map.loadPolitics("assets/politics/political_1989.gzp"));
  map.setReliefScale(0.f);
  std::vector<std::uint32_t> frame;
  map.render(960,600,frame,0,true,true);
  // Look up a verified-land vertex by projecting its actual lat/lon.
  // Warsaw 21E / 52N is in Poland on the source political mask.
  const float u=(21.f-(-20.f))/80.f,v=(76.f-52.f)/49.f;
  const float x=960.f*(.015f+u*.955f+(v-.5f)*.032f);
  const float y=600.f*(.048f+v*.906f+u*.032f);
  GZ_CHECK(map.selectAt(int(x),int(y),15.f));
  const auto selected=map.selection();GZ_CHECK(selected.has_value());
  GZ_CHECK(std::abs(selected->longitude-21.f)<1.f);
  GZ_CHECK(std::abs(selected->latitude-52.f)<1.f);
  GZ_CHECK(selected->politicalZone!=0);
  GZ_CHECK(!map.selectAt(-20,20));GZ_CHECK(map.selection().has_value());
  map.setView(1.6f,.10f,-.05f);
  map.render(960,600,frame,0,true,true);
  const float zx=960.f*.64f+(x-960.f*.64f)*1.6f+960.f*.1f;
  const float zy=600.f*.50f+(y-600.f*.50f)*1.6f-600.f*.05f;
  GZ_CHECK(map.selectAt(int(zx),int(zy),15.f));
  GZ_CHECK(std::abs(map.selection()->longitude-21.f)<1.f);
  map.clearSelection();GZ_CHECK(!map.selection().has_value());
  struct Vertex {float x,y,z;};
  Vertex points[3]={{0,0,0},{1,0,0},{0,1,0}};
  std::uint32_t indices[3]={0,1,2};
  GeometryView geo{points,sizeof(points),sizeof(Vertex),indices,3};
  GeometryStagingCache cache(1024);
  GZ_CHECK(cache.stage(18,geo)==GeometryStagingCache::Result::Stored);
  const auto version=cache.revision(18);
  GZ_CHECK(cache.view(18).valid());
  GZ_CHECK(cache.stage(18,geo)==GeometryStagingCache::Result::Unchanged);
  GZ_CHECK(cache.revision(18)==version);
  points[1].x=2.f;
  GZ_CHECK(cache.stage(18,geo)==GeometryStagingCache::Result::Stored);
  GZ_CHECK(cache.revision(18)>version&&cache.changedUploads()==2);
  indices[2]=99;
  GZ_CHECK(cache.stage(18,geo)==GeometryStagingCache::Result::Invalid);
  GZ_CHECK(cache.revision(18)>version);
  indices[2]=2;
  GeometryStagingCache tiny(8);
  GZ_CHECK(tiny.stage(1,geo)==GeometryStagingCache::Result::OverBudget);
  GZ_CHECK(cache.erase(18));GZ_CHECK(cache.residentBytes()==0&&!cache.view(18).valid());
  std::cout<<"v0.0.14: map picking, debug HUD, frame meter and staged geometry PASS\n";
}
