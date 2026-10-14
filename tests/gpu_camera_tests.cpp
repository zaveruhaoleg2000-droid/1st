#include "gz/GpuSceneMath.hpp"
#include "gz/RenderBackend.hpp"
#include "gz/SceneMesh.hpp"
#include "gz/NativeScene.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
static float mulRow(const float* m,int row,float x,float y,float z){
    return m[row]*x+m[4+row]*y+m[8+row]*z+m[12+row];
}
int main(){
    gz::GpuSceneCamera c{};c.enabled=true;c.focusX=3000;c.focusY=22;c.focusZ=1200;c.distance=400;c.pitch=.58f;c.yaw=.35f;
    auto u=gz::makeGpuFrameConstants(c,960,600,30.f);
    auto clipW=mulRow(u.viewProjection,3,c.focusX,c.focusY,c.focusZ);
    auto clipX=mulRow(u.viewProjection,0,c.focusX,c.focusY,c.focusZ);
    auto clipY=mulRow(u.viewProjection,1,c.focusX,c.focusY,c.focusZ);
    auto clipZ=mulRow(u.viewProjection,2,c.focusX,c.focusY,c.focusZ);
    assert(std::abs(clipW-c.distance)<.05f);
    assert(std::abs(clipX)<.005f*clipW);
    assert(std::abs(clipY)<.005f*clipW);
    assert(clipZ/clipW>0.f && clipZ/clipW<1.f);
    auto nearPoint=gz::makeGpuFrameConstants(c,1920,1080,30.f);
    assert(std::abs(nearPoint.viewProjection[0]-u.viewProjection[0])<.01f);
    gz::GeometryView gv{};
    assert(!gv.valid());
    gz::SceneMesh mesh{};gz::SceneMeshBuilder::addTree(mesh,10,0,20,1.f,128);
    gv={mesh.vertices.data(),mesh.vertices.size()*sizeof(gz::SceneVertex),sizeof(gz::SceneVertex),mesh.indices.data(),mesh.indices.size()};
    assert(gv.valid() && gv.strideBytes==48u);
    auto nativeView=gz::makeGeometryView(mesh);
    assert(nativeView.valid() && nativeView.indexCount==mesh.indices.size());
    auto camera=gz::makeGpuSceneCamera(gz::Camera{},3.f,{});
    assert(camera.enabled && std::isfinite(camera.focusY));
    std::cout<<"GPU camera projection, push constants and native mesh layout: PASS\n";
}
