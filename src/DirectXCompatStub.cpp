#include "gz/RenderBackend.hpp"
namespace gz {
namespace {
class DirectXCompatStub final : public IRenderBackend {
    RenderTelemetry stats_{};
public:
    GraphicsAPI api()const noexcept override{return GraphicsAPI::DirectXCompatibility;}
    bool initialize(const NativeRenderWindow&,const GraphicsLimits&,std::string& err) override{
        err="DirectX compatibility slot reserved; renderer deliberately disabled";return false;
    }
    bool resize(std::uint32_t,std::uint32_t,std::string& err) override {err="DirectX stub";return false;}
    bool submit(const RenderSubmission&,std::string& err) override {err="DirectX stub";return false;}
    void waitIdle()noexcept override{}
    bool ready()const noexcept override{return false;}
    const RenderTelemetry& telemetry()const noexcept override{return stats_;}
};
}
std::unique_ptr<IRenderBackend> makeDirectXCompatibilityStub(){return std::make_unique<DirectXCompatStub>();}
}
