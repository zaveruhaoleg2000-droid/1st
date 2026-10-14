#include "gz/SceneMesh.hpp"
#include <stdexcept>
#define CHECK(cond) do { if(!(cond)) throw std::runtime_error("Check failed: " #cond); } while(false)
#include <cmath>
#include <cstdint>
#include <iostream>
int main(){
    gz::SceneMesh tree;
    gz::SceneMeshBuilder::addTree(tree,10,3,20,1.f,135);
    CHECK(tree.treeCount==1 && tree.indices.size()/3==28);
    int bark=0,canopy=0,animated=0,anchored=0;
    for(const auto& v:tree.vertices){
        CHECK(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z));
        CHECK(std::isfinite(v.nx)&&std::isfinite(v.ny)&&std::isfinite(v.nz));
        CHECK(v.anchorX==10 && v.anchorZ==20);
        if(v.material==float(std::uint32_t(gz::TextureMaterial::TreeBark))){++bark;CHECK(v.windWeight==0);++anchored;}
        if(v.material==float(std::uint32_t(gz::TextureMaterial::Leaves))){++canopy;if(v.windWeight>.1f)++animated;}
    }
    CHECK(bark==36 && canopy==48 && anchored==bark && animated==canopy);
    gz::Simulation sim{4};sim.newOperation(96);
    gz::TerrainWorld world{9};world.setTileDirectory("assets/terrain");
    gz::Camera cam;cam.focusX=3350;cam.focusZ=1330;cam.distance=900;
    gz::SceneMesh scene;gz::SceneMeshBuilder{}.build(scene,world,sim,cam,0);
    CHECK(scene.terrainTriangleCount==160u*160u*2u);
    CHECK(scene.waterTriangleCount>0 && scene.treeCount>20);
    CHECK(scene.indices.size()%3==0 && scene.indices.size()>scene.terrainTriangleCount*3);
    CHECK(scene.shadowCasterCount>0 && scene.shadowIndexCount==scene.shadowCasterCount*6);
    CHECK(scene.shadowFirstIndex+scene.shadowIndexCount==scene.indices.size());
    CHECK(scene.bytes()<32u*1024u*1024u);
    for(auto idx:scene.indices)CHECK(idx<scene.vertices.size());
    for(const auto& v:scene.vertices){
        CHECK(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z));
        CHECK(std::isfinite(v.u)&&std::isfinite(v.v));
        CHECK(v.material>=0.f && v.material<float(std::uint32_t(gz::TextureMaterial::Count)));
    }
    std::cout<<"Geometry OK: "<<scene.indices.size()/3<<" tris, "<<scene.treeCount
             <<" trees, "<<scene.waterTriangleCount<<" water tris, "<<scene.bytes()/1024<<" KiB mesh\n";
}
