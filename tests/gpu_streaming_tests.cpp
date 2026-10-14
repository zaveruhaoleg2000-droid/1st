#include "TestCheck.hpp"
#include "gz/GeometryStaging.hpp"
#include "gz/NativeScene.hpp"
#include <cstring>
#include <iostream>

int main(){
    gz::Simulation sim(2);sim.newOperation(96);
    gz::TerrainWorld terrain(9);terrain.setTileDirectory("assets/terrain");
    gz::Camera camera;camera.focusX=3000;camera.focusZ=1250;camera.distance=900;
    gz::SceneMesh mesh;gz::SceneMeshBuilder builder;
    builder.build(mesh,terrain,sim,camera,0);
    auto view=gz::makeGeometryView(mesh);GZ_CHECK(view.valid());
    GZ_CHECK(view.staticVertexBytes>0 && view.staticVertexBytes<view.vertexBytes);
    GZ_CHECK(view.staticIndexCount>0 && view.staticIndexCount<view.indexCount);
    const auto staticRevision=view.staticRevision,dynamicRevision=view.dynamicRevision;
    const auto original=mesh;
    gz::MeshUploadTracker tracker;
    auto upload=tracker.plan(view);
    GZ_CHECK(upload.bytes()==view.vertexBytes+view.indexCount*4);
    tracker.commit(view);
    builder.build(mesh,terrain,sim,camera,0);view=gz::makeGeometryView(mesh);
    GZ_CHECK(builder.unitBuildCount()==1);
    GZ_CHECK(view.staticRevision==staticRevision && view.dynamicRevision==dynamicRevision);
    GZ_CHECK(tracker.plan(view).bytes()==0);
    camera.yaw+=.2f;camera.pitch+=.05f;
    builder.build(mesh,terrain,sim,camera,0);
    GZ_CHECK(builder.staticBuildCount()==1 && tracker.plan(gz::makeGeometryView(mesh)).bytes()==0);
    GZ_CHECK(builder.unitBuildCount()==1);
    sim.step(10);builder.build(mesh,terrain,sim,camera,0);view=gz::makeGeometryView(mesh);
    GZ_CHECK(view.staticRevision==staticRevision && view.dynamicRevision>dynamicRevision);
    GZ_CHECK(std::memcmp(mesh.vertices.data(),original.vertices.data(),view.staticVertexBytes)==0);
    upload=tracker.plan(view);
    GZ_CHECK(upload.vertices.offset==view.staticVertexBytes && upload.indices.offset==view.staticIndexCount*4);
    GZ_CHECK(upload.bytes()==view.vertexBytes-view.staticVertexBytes+(view.indexCount-view.staticIndexCount)*4);
    GZ_CHECK(upload.bytes()<view.vertexBytes/20);
    tracker.commit(view);
    const auto selectedRevision=view.dynamicRevision;
    builder.build(mesh,terrain,sim,camera,1);view=gz::makeGeometryView(mesh);
    GZ_CHECK(view.staticRevision==staticRevision && view.dynamicRevision>selectedRevision);
    tracker.commit(view);
    auto resized=tracker.plan(view,true,false);
    GZ_CHECK(resized.vertices.bytes==view.vertexBytes && resized.indices.bytes==0);
    resized=tracker.plan(view,false,true);
    GZ_CHECK(resized.vertices.bytes==0 && resized.indices.bytes==view.indexCount*4);
    camera.focusX+=25;builder.build(mesh,terrain,sim,camera,1);GZ_CHECK(mesh.staticRevision==staticRevision);
    camera.focusX+=128;builder.build(mesh,terrain,sim,camera,1);view=gz::makeGeometryView(mesh);
    GZ_CHECK(view.staticRevision>staticRevision && tracker.plan(view).vertices.offset==0);
    tracker.commit(view);
    auto revision=view.staticRevision;
    terrain.setTileDirectory("assets/terrain");builder.build(mesh,terrain,sim,camera,1);
    GZ_CHECK(mesh.staticRevision>revision);
    revision=mesh.staticRevision;
    gz::AtmosphereSettings atmosphere;atmosphere.shadows=false;
    builder.build(mesh,terrain,sim,camera,1,atmosphere);
    GZ_CHECK(mesh.staticRevision>revision && mesh.shadowIndexCount==0);
    revision=mesh.staticRevision;builder.invalidate();builder.build(mesh,terrain,sim,camera,1,atmosphere);
    GZ_CHECK(mesh.staticRevision>revision);
    gz::SceneMesh second;gz::SceneMeshBuilder other;
    other.build(second,terrain,sim,camera,1);
    GZ_CHECK(second.resourceId!=mesh.resourceId && tracker.plan(gz::makeGeometryView(second)).vertices.offset==0);
    camera.distance=28;camera.focusX=float(sim.soldier(0).x);camera.focusZ=float(sim.soldier(0).z);
    builder.build(mesh,terrain,sim,camera,0);view=gz::makeGeometryView(mesh);
    GZ_CHECK(view.valid());for(auto index:mesh.indices)GZ_CHECK(index<mesh.vertices.size());
    const auto nearRevision=mesh.staticRevision;
    camera.distance=35;builder.build(mesh,terrain,sim,camera,0);
    GZ_CHECK(mesh.staticRevision==nearRevision); // Projection-only zoom within the same LOD.
    tracker.commit(view);auto legacy=view;legacy.resourceId=0;
    GZ_CHECK(tracker.plan(legacy).bytes()==legacy.vertexBytes+legacy.indexCount*4);
    auto invalid=view;invalid.staticVertexBytes=view.vertexBytes+view.strideBytes;GZ_CHECK(!invalid.valid());
    invalid=view;invalid.staticIndexCount=view.indexCount+1;GZ_CHECK(!invalid.valid());
    invalid=view;invalid.staticVertexBytes=1;GZ_CHECK(!invalid.valid());
    tracker.reset();GZ_CHECK(tracker.plan(view).vertices.offset==0 && tracker.plan(view).bytes()>0);
    std::cout<<"GPU static cache, dirty ranges, camera/terrain invalidation and resource replacement: PASS\n";
}
