#include "gz/RenderBackend.hpp"
namespace gz {
namespace {
class VulkanUnavailable final : public IRenderBackend {
    RenderTelemetry stats_{};
public:
    GraphicsAPI api()const noexcept override{return GraphicsAPI::Vulkan;}
    bool initialize(const NativeRenderWindow&,const GraphicsLimits&,std::string& err)override{
        err="Vulkan backend not compiled (install Vulkan SDK and configure CMake)";return false;
    }
    bool resize(std::uint32_t,std::uint32_t,std::string& err)override{err="Vulkan SDK missing";return false;}
    bool submit(const RenderSubmission&,std::string& err)override{err="Vulkan SDK missing";return false;}
    void waitIdle()noexcept override{}
    bool ready()const noexcept override{return false;}
    const RenderTelemetry& telemetry()const noexcept override{return stats_;}
};
}
std::unique_ptr<IRenderBackend> makeVulkanBackend(){return std::make_unique<VulkanUnavailable>();}
}
