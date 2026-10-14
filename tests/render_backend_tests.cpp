#include "gz/RenderBackend.hpp"
#ifdef NDEBUG
#undef NDEBUG // Test assertions must execute in optimized builds, too.
#endif
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
int main(){
    using namespace gz;
    static_assert(sizeof(std::uint32_t)==4);
    assert(std::string(graphicsAPIName(GraphicsAPI::Vulkan))=="Vulkan");
    assert(std::string(graphicsAPIName(GraphicsAPI::DirectXCompatibility)).find("placeholder")!=std::string::npos);
    RenderBackendHost host;
    NativeRenderWindow fake{WindowSystem::Xlib,nullptr,1,960,600};
    assert(host.initialize(fake,GraphicsAPI::Software));
    assert(host.activeAPI()==GraphicsAPI::Software);
    assert(!host.usingGPU());
    assert(host.resize(1280,720));
    std::uint32_t pixels[4]={0xff113355,0xffaa8844,0xff000000,0xffffffff};
    FramePixels frame{pixels,2,2,2,PixelFormat::BGRA8};
    assert(frame.valid());
    assert(!host.present(frame)); // software bridge delegates OS blit to caller
    assert(host.telemetry().resizeCount==1);
    assert(!host.select(GraphicsAPI::DirectXCompatibility));
    assert(host.activeAPI()==GraphicsAPI::Software); // non-destructive rejection
    assert(host.error().find("DirectX")!=std::string::npos);
    assert((!FramePixels{nullptr,2,2,2,PixelFormat::BGRA8}.valid()));
    assert((!FramePixels{pixels,3,2,2,PixelFormat::BGRA8}.valid()));
    gz::GeometryView invalidGeometry{};
    assert(!invalidGeometry.valid());
    gz::GeometryView geometry{pixels,sizeof(pixels),sizeof(std::uint32_t),pixels,4};
    assert(geometry.valid());
    assert(!host.submit({frame,geometry,12,1.5f}));
    // Native Vulkan may fail on headless test hosts. A failure must always
    // produce a software bridge, not a null renderer or an exception.
    host.shutdown();
    assert(host.initialize(fake,GraphicsAPI::Automatic));
    assert(host.activeAPI()==GraphicsAPI::Software);
    assert(!host.select(GraphicsAPI::Vulkan));
    assert(host.activeAPI()==GraphicsAPI::Software);
    assert(host.initialize(fake,GraphicsAPI::DirectXCompatibility));
    assert(host.activeAPI()==GraphicsAPI::Software);
    assert(!host.initialize({WindowSystem::Xlib,nullptr,0,0,0},GraphicsAPI::Automatic));
    std::cout<<"RHI renderer selection/fallback/format/resize: PASS\n";
}
