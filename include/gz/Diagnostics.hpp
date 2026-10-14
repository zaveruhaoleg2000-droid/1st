#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace gz {
struct CampaignState;
struct DiagnosticSnapshot {
    float fps=0.f,frameMs=0.f,sceneMs=0.f;
    std::uint64_t triangles=0,uploadedBytes=0;
    std::size_t terrainCacheBytes=0;
    std::string renderer="Software";
};
// O(1) moving window; records measured work time, not the artificial frame limiter.
class FrameMeter {
public:
    void record(float sceneMs,float frameMs) noexcept;
    DiagnosticSnapshot snapshot(std::uint64_t triangles,std::size_t cacheBytes,
                                 std::uint64_t uploadedBytes,const char* renderer)const;
    std::size_t sampleCount()const noexcept{return count_;}
private:
    struct Sample {float scene=0,frame=0;};
    std::array<Sample,60> samples_{};
    std::size_t cursor_=0,count_=0;
    double sumScene_=0,sumFrame_=0;
};
// Text is drawn inside the same framebuffer that Vulkan presents. This avoids
// relying on GDI/X11 overlays that disappear when a GPU owns the window.
class DebugHud {
public:
    bool load(const std::string& glyphPgm);
    bool ready()const noexcept{return !glyphs_.empty();}
    void draw(std::uint32_t* pixels,int width,int height,const DiagnosticSnapshot& s)const;
    void drawContract(std::uint32_t* pixels,int width,int height,const CampaignState& s)const;
private:
    void write(std::uint32_t* pixels,int w,int h,int x,int y,const std::string& text,
               std::uint32_t rgb)const;
    std::vector<std::uint8_t> glyphs_;
    int glyphW_=0,glyphH_=0;
};
}
