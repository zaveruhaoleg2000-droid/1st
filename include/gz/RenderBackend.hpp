#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include "gz/GpuSceneMath.hpp"

namespace gz {
// Render Hardware Interface v1. No OS/Vulkan/DirectX headers cross this boundary.
// All game systems keep their own CPU-side state and pass immutable snapshots.
enum class GraphicsAPI : std::uint8_t { Automatic, Vulkan, DirectXCompatibility, Software };
enum class WindowSystem : std::uint8_t { Win32, Xlib };
enum class PixelFormat : std::uint8_t { BGRA8 }; // each uint32 pixel is 0xAARRGGBB on little-endian hosts
struct NativeRenderWindow {
    WindowSystem system=WindowSystem::Xlib;
    void* display=nullptr; // Xlib Display*, unused on Windows
    std::uintptr_t window=0; // X11 Window or Win32 HWND
    std::uint32_t width=0,height=0;
};
struct FramePixels {
    const std::uint32_t* data=nullptr;
    std::uint32_t width=0,height=0,stridePixels=0;
    PixelFormat format=PixelFormat::BGRA8;
    bool valid() const noexcept {
        return data && width>0 && height>0 && stridePixels>=width &&
            width<=8192 && height<=8192;
    }
};
// Future native draw stream: the Vulkan implementation will upload these
// resources to device-local memory; ownership remains with the simulation.
struct GeometryView {
    const void* vertices=nullptr;
    std::size_t vertexBytes=0;
    std::size_t strideBytes=0;
    const std::uint32_t* indices=nullptr;
    std::size_t indexCount=0;
    bool valid()const noexcept{
        return vertices && vertexBytes && strideBytes && vertexBytes%strideBytes==0 &&
            indices && indexCount && indexCount<=16'000'000;
    }
};
struct RenderSubmission {
    FramePixels fallbackFrame{};
    GeometryView worldMesh{}; // optional until GPU native scene pipeline is enabled
    std::uint64_t simulationTick=0;
    float visualTimeSeconds=0;
    GpuSceneCamera camera{};
};
struct GraphicsLimits {
    // Target profile: 4 CPU threads, 4 GiB RAM, 2 GiB video memory.
    std::uint32_t maxFramesInFlight=2;
    std::uint32_t maxTextureDimension=4096;
    std::size_t streamingBudgetBytes=128u*1024u*1024u;
};
struct RenderTelemetry {
    std::uint64_t completedFrames=0;
    std::uint64_t uploadedBytes=0;
    std::uint64_t resizeCount=0;
    std::uint64_t failedPresents=0;
    std::uint64_t nativeSceneFrames=0;
    std::uint64_t nativeSceneTriangles=0;
    std::uint64_t meshUploadedBytes=0;
};
class IRenderBackend {
public:
    virtual ~IRenderBackend()=default;
    virtual GraphicsAPI api() const noexcept=0;
    virtual bool initialize(const NativeRenderWindow&,const GraphicsLimits&,std::string& error)=0;
    virtual bool resize(std::uint32_t width,std::uint32_t height,std::string& error)=0;
    // false means the caller should display the CPU framebuffer itself.
    virtual bool submit(const RenderSubmission& frame,std::string& error)=0;
    virtual void waitIdle() noexcept=0;
    virtual bool ready() const noexcept=0;
    virtual bool supportsNativeScene() const noexcept{return false;}
    virtual const RenderTelemetry& telemetry() const noexcept=0;
};
// Every backend is compiled independently; the Vulkan SDK is optional for developers
// but required to build an enabled Vulkan renderer on Windows or Linux.
std::unique_ptr<IRenderBackend> makeVulkanBackend();
std::unique_ptr<IRenderBackend> makeDirectXCompatibilityStub();
std::unique_ptr<IRenderBackend> makeSoftwareBridge();

class RenderBackendHost {
public:
    RenderBackendHost()=default;
    ~RenderBackendHost();
    RenderBackendHost(const RenderBackendHost&)=delete;
    RenderBackendHost& operator=(const RenderBackendHost&)=delete;
    bool initialize(NativeRenderWindow native, GraphicsAPI preference=GraphicsAPI::Automatic,
                    GraphicsLimits limits={});
    bool resize(std::uint32_t width,std::uint32_t height);
    // Returns true only when the GPU actually presented the frame.
    bool submit(const RenderSubmission& submission);
    bool present(const FramePixels& pixels){return submit(RenderSubmission{pixels,{},{},0.f});}
    bool select(GraphicsAPI requested);
    GraphicsAPI activeAPI() const noexcept;
    GraphicsAPI requestedAPI() const noexcept {return preference_;}
    std::string_view error() const noexcept {return error_;}
    const RenderTelemetry& telemetry() const noexcept;
    bool usingGPU() const noexcept {return activeAPI()==GraphicsAPI::Vulkan;}
    bool nativeSceneReady() const noexcept {return backend_ && backend_->supportsNativeScene();}
    void shutdown() noexcept;
private:
    bool activate(GraphicsAPI api);
    NativeRenderWindow window_{};
    GraphicsLimits limits_{};
    GraphicsAPI preference_=GraphicsAPI::Automatic;
    std::unique_ptr<IRenderBackend> backend_;
    std::string error_;
    RenderTelemetry empty_{};
};
const char* graphicsAPIName(GraphicsAPI api) noexcept;
} // namespace gz
