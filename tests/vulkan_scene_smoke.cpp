#include "TestCheck.hpp"
#include "gz/NativeScene.hpp"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>
#include <thread>
#include <vector>

static std::vector<unsigned long> capture(Display* display,Window window,int width,int height,const char* path){
    std::this_thread::sleep_for(std::chrono::milliseconds(50));XSync(display,False);
    auto* image=XGetImage(display,window,0,0,width,height,AllPlanes,ZPixmap);GZ_CHECK(image);
    std::vector<unsigned long> pixels;
    std::ofstream out(path,std::ios::binary);GZ_CHECK(bool(out));
    out<<"P6\n"<<width<<' '<<height<<"\n255\n";
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){
        const auto p=XGetPixel(image,x,y);pixels.push_back(p);
        const char rgb[]={char((p>>16)&255),char((p>>8)&255),char(p&255)};out.write(rgb,3);
    }
    XDestroyImage(image);GZ_CHECK(bool(out));
    GZ_CHECK(std::set<unsigned long>(pixels.begin(),pixels.end()).size()>8);
    return pixels;
}
int main(){
    XInitThreads();auto* display=XOpenDisplay(nullptr);GZ_CHECK(display);
    const bool gallery=std::getenv("GZ_VISUAL_CAPTURE")!=nullptr;
    const int width=gallery?1280:320,height=gallery?720:240;
    const auto window=XCreateSimpleWindow(display,DefaultRootWindow(display),0,0,width,height,0,0,0);
    XMapWindow(display,window);XSync(display,False);
    gz::RenderBackendHost host;
    GZ_CHECK(host.initialize({gz::WindowSystem::Xlib,display,window,width,height},gz::GraphicsAPI::Vulkan));
    if(!host.nativeSceneReady())std::cerr<<"Native Vulkan unavailable: "<<host.error()<<'\n';
    GZ_CHECK(host.activeAPI()==gz::GraphicsAPI::Vulkan && host.nativeSceneReady());
    gz::Simulation sim(2);sim.newOperation(96);
    gz::TerrainWorld terrain(9);terrain.setTileDirectory("assets/terrain");
    gz::Camera camera;camera.focusX=3000;camera.focusZ=1250;camera.distance=900;
    gz::SceneMesh mesh;gz::SceneMeshBuilder builder;
    gz::AtmosphereSettings atmosphere{};
    float visualTime=7;
    std::vector<std::uint32_t> sentinel(width*height,0xffff00ffu);
    auto submit=[&](){
        builder.build(mesh,terrain,sim,camera,0,atmosphere);
        gz::RenderSubmission frame;
        frame.fallbackFrame={sentinel.data(),width,height,width,gz::PixelFormat::BGRA8};
        frame.worldMesh=gz::makeGeometryView(mesh);
        frame.camera=gz::makeGpuSceneCamera(camera,terrain.heightAt(camera.focusX,camera.focusZ),atmosphere);
        frame.visualTimeSeconds=visualTime;
        GZ_CHECK(host.submit(frame));GZ_CHECK(host.activeAPI()==gz::GraphicsAPI::Vulkan);
    };
    submit();const auto first=host.telemetry().meshUploadedBytes;
    GZ_CHECK(first==gz::makeGeometryView(mesh).vertexBytes+gz::makeGeometryView(mesh).indexCount*4);
    for(int i=0;i<8;++i)submit();
    GZ_CHECK(host.telemetry().meshUploadedBytes==first && host.telemetry().skippedMeshUploads==8);
    const auto original=capture(display,window,width,height,"test_build_vulkan_static.ppm");
    sim.step(30);submit();
    const auto view=gz::makeGeometryView(mesh);
    const auto delta=host.telemetry().meshUploadedBytes-first;
    GZ_CHECK(delta==view.vertexBytes-view.staticVertexBytes+(view.indexCount-view.staticIndexCount)*4);
    GZ_CHECK(delta>0 && delta<first/10);
    const auto moved=capture(display,window,width,height,"test_build_vulkan_units.ppm");GZ_CHECK(moved!=original);
    auto before=host.telemetry().meshUploadedBytes;
    camera.yaw+=.3f;submit();GZ_CHECK(host.telemetry().meshUploadedBytes==before);
    camera.focusX+=25;submit();GZ_CHECK(host.telemetry().meshUploadedBytes>before+first/2);
    before=host.telemetry().meshUploadedBytes;
    GZ_CHECK(host.resize(width,height) && host.nativeSceneReady());submit();
    GZ_CHECK(host.telemetry().meshUploadedBytes>before+first/2);
    GZ_CHECK(host.telemetry().nativeSceneFrames==13);
    GZ_CHECK(host.telemetry().meshResidentBytes<=128u*1024u*1024u);
    GZ_CHECK(host.telemetry().validationErrors==0);
    GZ_CHECK(host.telemetry().materialTextureCount==16);
    GZ_CHECK(host.telemetry().graphicsResidentBytes<256u*1024u*1024u);
    std::cout<<"Native Vulkan: 13 frames, first upload="<<first<<" bytes, unit update="<<delta
        <<" bytes, skipped="<<host.telemetry().skippedMeshUploads
        <<", resident="<<host.telemetry().meshResidentBytes<<" bytes; actual X11 native frames captured; validation errors=0\n";
    // Compare actual shader results for lighting, weather and shadow toggles.
    camera.focusX=3055.5f;camera.focusZ=1258.f;camera.distance=28;camera.pitch=.36f;camera.yaw=.48f;
    atmosphere.time=gz::TimeOfDay::Sunset;submit();
    const auto sunset=capture(display,window,width,height,"test_build_vulkan_vehicle_sunset.ppm");
    GZ_CHECK(mesh.vehicleModelCount>0 && mesh.soldierModelCount>0);
    atmosphere.shadows=false;submit();
    const auto unshadowed=capture(display,window,width,height,"test_build_vulkan_no_shadows.ppm");GZ_CHECK(unshadowed!=sunset);
    atmosphere={gz::Weather::Rain,gz::TimeOfDay::Noon,true};submit();
    const auto rain=capture(display,window,width,height,"test_build_vulkan_vehicle_rain.ppm");GZ_CHECK(rain!=sunset);
    atmosphere={gz::Weather::Snow,gz::TimeOfDay::Noon,true};submit();
    const auto snow=capture(display,window,width,height,"test_build_vulkan_vehicle_snow.ppm");GZ_CHECK(snow!=rain);
    atmosphere={};camera.focusX=float(sim.soldier(0).x);camera.focusZ=float(sim.soldier(0).z);
    camera.distance=10;camera.pitch=.32f;submit();
    capture(display,window,width,height,"test_build_vulkan_soldiers.ppm");
    camera.focusX=3350;camera.focusZ=1330;camera.distance=65;camera.pitch=.30f;camera.yaw=.45f;
    atmosphere={gz::Weather::Clear,gz::TimeOfDay::Sunset,true};submit();
    const auto water=capture(display,window,width,height,"test_build_vulkan_water.ppm");
    visualTime+=4;submit();
    GZ_CHECK(capture(display,window,width,height,"test_build_vulkan_water_animated.ppm")!=water);
    GZ_CHECK(host.telemetry().validationErrors==0);
    std::cout<<"Native material/weather gallery: "<<host.telemetry().materialTextureCount<<" textures; "
        <<host.telemetry().shadowMapSize<<"px shadow map; MSAA "<<host.telemetry().msaaSamples
        <<"x; graphics allocations="<<host.telemetry().graphicsResidentBytes<<" bytes; validation errors=0\n";
    host.shutdown();XDestroyWindow(display,window);XCloseDisplay(display);
}
