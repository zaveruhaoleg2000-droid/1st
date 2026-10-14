#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
namespace gz {
// Cross-API execution plan. CPU presentation currently activates two passes.
// GPU-native plans are validated now, and can be executed one by one later.
enum class RenderStage : std::uint8_t {
    Upload, Depth, Terrain, StaticGeometry, Units, Vegetation,
    Water, DynamicLighting, WeatherFX, Interface, Present
};
struct RenderPass {
    RenderStage stage;
    std::vector<RenderStage> mustFollow;
};
class RenderGraphPlan {
public:
    static RenderGraphPlan cpuFrameUpload();
    static RenderGraphPlan nativeScene();
    bool validate(std::string& error)const;
    const std::vector<RenderPass>& passes()const noexcept {return passes_;}
private:
    std::vector<RenderPass> passes_;
};
const char* renderStageName(RenderStage stage) noexcept;

// Budget ledger: no driver calls, safe for deterministic tests. Future GPU
// residency manager must reserve space before uploading an asset/chunk.
class GpuBudgetLedger {
public:
    explicit GpuBudgetLedger(std::size_t limitBytes):limit_(limitBytes){}
    bool acquire(std::uint64_t resourceId,std::size_t bytes);
    bool release(std::uint64_t resourceId);
    std::size_t inUse()const noexcept {return used_;}
    std::size_t limit()const noexcept {return limit_;}
    std::size_t count()const noexcept {return resources_.size();}
private:
    std::size_t limit_=0,used_=0;
    std::vector<std::pair<std::uint64_t,std::size_t>> resources_;
};
}
