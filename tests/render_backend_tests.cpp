#include "gz/RenderBackend.hpp"
#include "TestCheck.hpp"
#include <cstdint>
#include <iostream>
#include <string>
int main(){
    using namespace gz;
    static_assert(sizeof(std::uint32_t)==4);
    GZ_CHECK(std::string(graphicsAPIName(GraphicsAPI::Vulkan))=="Vulkan");
    GZ_CHECK(std::string(graphicsAPIName(GraphicsAPI::DirectXCompatibility)).find("placeholder")!=std::string::npos);
    RenderBackendHost host;
    NativeRenderWindow fake{WindowSystem::Xlib,nullptr,1,960,600};
    GZ_CHECK(host.initialize(fake,GraphicsAPI::Software));
    GZ_CHECK(host.activeAPI()==GraphicsAPI::Software);
    GZ_CHECK(!host.usingGPU());
    GZ_CHECK(host.resize(1280,720));
    std::uint32_t pixels[4]={0xff113355,0xffaa8844,0xff000000,0xffffffff};
    FramePixels frame{pixels,2,2,2,PixelFormat::BGRA8};
    GZ_CHECK(frame.valid());
    GZ_CHECK(!host.present(frame)); // software bridge delegates OS blit to caller
    GZ_CHECK(host.telemetry().resizeCount==1);
    GZ_CHECK(!host.select(GraphicsAPI::DirectXCompatibility));
    GZ_CHECK(host.activeAPI()==GraphicsAPI::Software); // non-destructive rejection
    GZ_CHECK(host.error().find("DirectX")!=std::string::npos);
    GZ_CHECK((!FramePixels{nullptr,2,2,2,PixelFormat::BGRA8}.valid()));
    GZ_CHECK((!FramePixels{pixels,3,2,2,PixelFormat::BGRA8}.valid()));
    gz::GeometryView invalidGeometry{};
    GZ_CHECK(!invalidGeometry.valid());
    gz::GeometryView geometry{pixels,sizeof(pixels),sizeof(std::uint32_t),pixels,4};
    GZ_CHECK(geometry.valid());
    GZ_CHECK(!host.submit({frame,geometry,12,1.5f}));
    // Native Vulkan may fail on headless test hosts. A failure must always
    // produce a software bridge, not a null renderer or an exception.
    host.shutdown();
    GZ_CHECK(host.initialize(fake,GraphicsAPI::Automatic));
    GZ_CHECK(host.activeAPI()==GraphicsAPI::Software);
    GZ_CHECK(!host.select(GraphicsAPI::Vulkan));
    GZ_CHECK(host.activeAPI()==GraphicsAPI::Software);
    GZ_CHECK(host.initialize(fake,GraphicsAPI::DirectXCompatibility));
    GZ_CHECK(host.activeAPI()==GraphicsAPI::Software);
    GZ_CHECK(!host.initialize({WindowSystem::Xlib,nullptr,0,0,0},GraphicsAPI::Automatic));
    std::cout<<"RHI renderer selection/fallback/format/resize: PASS\n";
}
