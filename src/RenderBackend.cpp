#include "gz/RenderBackend.hpp"
#include <algorithm>
#include <utility>
namespace gz {
const char* graphicsAPIName(GraphicsAPI api) noexcept {
    switch(api){case GraphicsAPI::Automatic:return "Auto";
        case GraphicsAPI::Vulkan:return "Vulkan";
        case GraphicsAPI::DirectXCompatibility:return "DirectX (placeholder)";
        case GraphicsAPI::Software:return "Software";}
    return "Unknown";
}
namespace {
class SoftwareBridge final : public IRenderBackend {
    bool initialized_=false;
    RenderTelemetry stats_{};
public:
    GraphicsAPI api()const noexcept override{return GraphicsAPI::Software;}
    bool initialize(const NativeRenderWindow& w,const GraphicsLimits&,std::string& error)override{
        initialized_=w.window!=0 && w.width && w.height;
        if(!initialized_)error="Invalid native window for software bridge";
        return initialized_;
    }
    bool resize(std::uint32_t w,std::uint32_t h,std::string& error)override{
        if(!w||!h){error="Invalid software backbuffer size";return false;}
        ++stats_.resizeCount;return initialized_;
    }
    bool submit(const RenderSubmission&,std::string&)override{return false;}
    void waitIdle()noexcept override{}
    bool ready()const noexcept override{return initialized_;}
    const RenderTelemetry& telemetry()const noexcept override{return stats_;}
};
}
std::unique_ptr<IRenderBackend> makeSoftwareBridge(){return std::make_unique<SoftwareBridge>();}
RenderBackendHost::~RenderBackendHost(){shutdown();}
void RenderBackendHost::shutdown()noexcept{
    if(backend_)backend_->waitIdle();
    backend_.reset();
}
bool RenderBackendHost::activate(GraphicsAPI api){
    std::unique_ptr<IRenderBackend> candidate;
    switch(api){
    case GraphicsAPI::Vulkan:candidate=makeVulkanBackend();break;
    case GraphicsAPI::DirectXCompatibility:candidate=makeDirectXCompatibilityStub();break;
    case GraphicsAPI::Software:candidate=makeSoftwareBridge();break;
    default:return false;
    }
    if(!candidate){error_="Renderer factory unavailable";return false;}
    std::string reason;
    if(!candidate->initialize(window_,limits_,reason)){
        error_=std::string(graphicsAPIName(api))+": "+reason;
        return false; // the currently running backend remains untouched
    }
    if(backend_)backend_->waitIdle();
    backend_=std::move(candidate);
    return true;
}
bool RenderBackendHost::initialize(NativeRenderWindow window,GraphicsAPI preference,GraphicsLimits limits){
    shutdown();window_=window;limits_=limits;preference_=preference;error_.clear();
    if(window.width==0||window.height==0||window.window==0){error_="Native window is missing";return false;}
    if(preference==GraphicsAPI::Automatic){if(activate(GraphicsAPI::Vulkan))return true;}
    else if(activate(preference))return true;
    const std::string failure=error_;
    if(!activate(GraphicsAPI::Software))return false;
    if(!failure.empty())error_=failure+"; using software renderer";
    return true;
}
bool RenderBackendHost::resize(std::uint32_t width,std::uint32_t height){
    if(!width||!height)return false;
    window_.width=width;window_.height=height;
    if(!backend_)return false;
    std::string reason;
    if(backend_->resize(width,height,reason))return true;
    error_="Resize failed: "+reason;
    if(backend_->api()!=GraphicsAPI::Software)activate(GraphicsAPI::Software);
    return false;
}
bool RenderBackendHost::submit(const RenderSubmission& submission){
    if(!submission.fallbackFrame.valid()||!backend_)return false;
    std::string reason;
    if(backend_->submit(submission,reason))return true;
    if(backend_->api()==GraphicsAPI::Software)return false; // handled externally
    error_="GPU present failed: "+reason;
    activate(GraphicsAPI::Software);
    return false;
}
bool RenderBackendHost::select(GraphicsAPI requested){
    preference_=requested;
    if(requested==GraphicsAPI::Automatic)requested=GraphicsAPI::Vulkan;
    if(backend_ && backend_->api()==requested)return true;
    if(activate(requested)){error_.clear();return true;}
    if(!backend_)activate(GraphicsAPI::Software);
    return false;
}
GraphicsAPI RenderBackendHost::activeAPI()const noexcept{return backend_?backend_->api():GraphicsAPI::Software;}
const RenderTelemetry& RenderBackendHost::telemetry()const noexcept{return backend_?backend_->telemetry():empty_;}
}
