#include "gz/GpuSceneMath.hpp"
#include "gz/RenderBackend.hpp"
#include "gz/SceneMesh.hpp"
#include "gz/NativeScene.hpp"
#include "TestCheck.hpp"
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
    GZ_CHECK(std::abs(clipW-c.distance)<.05f);
    GZ_CHECK(std::abs(clipX)<.005f*clipW);
    GZ_CHECK(std::abs(clipY)<.005f*clipW);
    GZ_CHECK(clipZ/clipW>0.f && clipZ/clipW<1.f);
    const auto doubled=gz::makeGpuFrameConstants(c,1920,1200,30.f);
    for(int i=0;i<16;++i)
        GZ_CHECK(std::abs(doubled.viewProjection[i]-u.viewProjection[i])<.01f);
    // A wider aspect changes the horizontal scale, not the vertical projection.
    const auto widescreen=gz::makeGpuFrameConstants(c,1920,1080,30.f);
    for(int column=0;column<4;++column) {
        GZ_CHECK(std::abs(widescreen.viewProjection[column*4]-.9f*u.viewProjection[column*4])<.01f);
        for(int row=1;row<4;++row)
            GZ_CHECK(std::abs(widescreen.viewProjection[column*4+row]-u.viewProjection[column*4+row])<.01f);
    }
    gz::GeometryView gv{};
    GZ_CHECK(!gv.valid());
    gz::SceneMesh mesh{};gz::SceneMeshBuilder::addTree(mesh,10,0,20,1.f,128);
    gv={mesh.vertices.data(),mesh.vertices.size()*sizeof(gz::SceneVertex),sizeof(gz::SceneVertex),mesh.indices.data(),mesh.indices.size()};
    GZ_CHECK(gv.valid() && gv.strideBytes==48u);
    auto nativeView=gz::makeGeometryView(mesh);
    GZ_CHECK(nativeView.valid() && nativeView.indexCount==mesh.indices.size());
    auto camera=gz::makeGpuSceneCamera(gz::Camera{},3.f,{});
    GZ_CHECK(camera.enabled && std::isfinite(camera.focusY));
    auto sunset=gz::makeGpuSceneCamera(gz::Camera{},3.f,{gz::Weather::Rain,gz::TimeOfDay::Sunset,false});
    GZ_CHECK(sunset.sunR>sunset.sunB && sunset.wetness>.5f && sunset.shadowStrength==0.f);
    auto rainConstants=gz::makeGpuFrameConstants(sunset,960,540,3.f);
    GZ_CHECK(rainConstants.sky[3]==sunset.wetness && rainConstants.sunColor[3]==0.f);
    GZ_CHECK(std::abs(mulRow(rainConstants.shadowProjection,0,sunset.focusX,sunset.focusY,sunset.focusZ))<.001f);
    GZ_CHECK(std::abs(mulRow(rainConstants.shadowProjection,1,sunset.focusX,sunset.focusY,sunset.focusZ))<.001f);
    GZ_CHECK(std::abs(mulRow(rainConstants.shadowProjection,2,sunset.focusX,sunset.focusY,sunset.focusZ)-.5f)<.001f);
    gz::VisualEffects disabled;disabled.wind=false;disabled.waterReflections=false;
    auto calm=gz::makeGpuSceneCamera(gz::Camera{},3.f,{},disabled);
    GZ_CHECK(calm.wind==0.f && !calm.waterReflections);
    std::cout<<"GPU camera, shadow projection, weather uniforms and native mesh layout: PASS\n";
}
