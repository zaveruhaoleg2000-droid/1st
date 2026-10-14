#include "gz/Sprites.hpp"
#include "TestCheck.hpp"
#include <limits>
#include <cstring>
#include <iostream>
int main(){
    gz::SpriteAtlas atlas;GZ_CHECK(atlas.load());GZ_CHECK(atlas.resolution==256&&atlas.layers==16&&atlas.pixels.size()==256u*256u*16u*4u);
    GZ_CHECK(atlas.sample(gz::SpriteKind::Broadleaf,0,0)[3]==0);GZ_CHECK(atlas.sample(gz::SpriteKind::Cloud,.5f,.5f)[3]>.1f);
    GZ_CHECK(!atlas.load("missing.gzsa"));
    gz::Camera camera;camera.focusX=camera.focusZ=0;camera.distance=20;camera.yaw=0;
    gz::SpriteBatch batch;GZ_CHECK(batch.add({0,1,5,2,2,gz::SpriteKind::Smoke,{1,1,1,.4f},false}));GZ_CHECK(batch.add({0,1,15,2,2,gz::SpriteKind::Smoke,{1,1,1,.6f},false}));
    GZ_CHECK(!batch.add({0,1,10,-2,2}));GZ_CHECK(!batch.add({std::numeric_limits<float>::quiet_NaN(),1,10,2,2}));batch.finish(camera);
    GZ_CHECK(batch.sortedSprites()[0].z==15);auto view=batch.geometry();GZ_CHECK(view.valid()&&view.indexCount==12&&view.strideBytes==40);
    const auto* vertices=static_cast<const gz::SpriteVertex*>(view.vertices);GZ_CHECK(vertices[0].a==.6f);for(std::size_t i=0;i<view.indexCount;++i)GZ_CHECK(view.indices[i]<8);
    for(unsigned weapon=0;weapon<unsigned(gz::Weapon::Count);++weapon){std::size_t previous=std::numeric_limits<std::size_t>::max();
        for(int level=0;level<4;++level){gz::SceneMesh m;gz::SceneMeshBuilder::addWeapon(m,gz::Weapon(weapon),true,gz::ModelLod(level));GZ_CHECK(m.indices.size()<previous&&!m.indices.empty());previous=m.indices.size();for(auto i:m.indices)GZ_CHECK(i<m.vertices.size());}}
    for(int kind=-1;kind<3;++kind){std::size_t previous=std::numeric_limits<std::size_t>::max();for(int level=0;level<4;++level){gz::SceneMesh m;if(kind<0)gz::SceneMeshBuilder::addSoldierForLod(m,gz::ModelLod(level));else gz::SceneMeshBuilder::addVehicleForLod(m,kind,gz::ModelLod(level));GZ_CHECK(m.indices.size()<previous);previous=m.indices.size();}}
    GZ_CHECK(gz::modelLod(20)==gz::ModelLod::Near&&gz::modelLod(60)==gz::ModelLod::Medium&&gz::modelLod(200)==gz::ModelLod::Far&&gz::modelLod(400)==gz::ModelLod::Silhouette);
    GZ_CHECK(gz::stableModelLod(42,gz::ModelLod::Near)==gz::ModelLod::Near);GZ_CHECK(gz::stableModelLod(46,gz::ModelLod::Near)==gz::ModelLod::Medium);GZ_CHECK(gz::stableModelLod(36,gz::ModelLod::Medium)==gz::ModelLod::Medium);GZ_CHECK(gz::stableModelLod(34,gz::ModelLod::Medium)==gz::ModelLod::Near);
    gz::TerrainWorld world(8);world.setTileDirectory("assets/terrain");gz::Simulation sim(1);sim.newOperation(48);camera.focusX=2800;camera.focusZ=1100;camera.distance=15;
    batch.build(world,sim,camera,{},0,2);GZ_CHECK(batch.count()>100&&batch.count()<=4096);view=batch.geometry();GZ_CHECK(view.vertexBytes+view.indexCount*4<4u*1024*1024);
    bool grass=false,trees=false,clouds=false;for(auto s:batch.sortedSprites()){grass|=s.kind==gz::SpriteKind::Grass;trees|=s.kind==gz::SpriteKind::Broadleaf||s.kind==gz::SpriteKind::Leaf;clouds|=s.kind==gz::SpriteKind::Cloud;}
    GZ_CHECK(grass&&trees&&clouds);sim.setMode(gz::ViewMode::Direct);sim.setDirectInput({});GZ_CHECK(sim.fireControlled());batch.build(world,sim,camera,{},.1f,2);
    bool muzzle=false,tracer=false,smoke=false;for(auto s:batch.sortedSprites()){muzzle|=s.kind==gz::SpriteKind::Muzzle;tracer|=s.kind==gz::SpriteKind::Spark;smoke|=s.kind==gz::SpriteKind::Smoke;}GZ_CHECK(muzzle&&tracer&&smoke);
    std::cout<<"RGBA transparent sprites, depth sorting, bounded foliage/clouds/weapon FX, 4 distinct weapon/soldier/vehicle LODs and hysteresis: PASS\n";
}
