#pragma once
#include "gz/Textures.hpp"
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

namespace gz {
// Shared CPU/GPU quality policy. These values are saved as integers (0..2).
enum class ShaderQuality : std::uint8_t { Eco=0, Balanced=1, Quality=2 };
enum class ShaderSurface : std::uint8_t { Terrain, Foliage, Architecture, Water, Shadow, Count };
struct ShaderBudget {
    int localLights=0;
    bool wetHighlights=false;
    bool canopyDetail=false;
    bool waterHighlights=false;
    bool fineTerrain=false;
};
constexpr ShaderBudget shaderBudget(ShaderQuality q) {
    switch(q) {
    case ShaderQuality::Eco:      return {0,false,false,false,false};
    case ShaderQuality::Quality:  return {4,true,true,true,true};
    default:                      return {4,true,false,true,false};
    }
}
constexpr const char* shaderQualityName(ShaderQuality q) {
    return q==ShaderQuality::Eco?"ECO":q==ShaderQuality::Quality?"QUALITY":"BALANCED";
}
constexpr ShaderQuality shaderQualityFromInt(int value) {
    return value<=0?ShaderQuality::Eco:value>=2?ShaderQuality::Quality:ShaderQuality::Balanced;
}
constexpr ShaderSurface shaderSurface(TextureMaterial material) {
    switch(material) {
    case TextureMaterial::TreeBark: case TextureMaterial::Leaves: return ShaderSurface::Foliage;
    case TextureMaterial::Brick: case TextureMaterial::Concrete: case TextureMaterial::WoodPlanks:
    case TextureMaterial::Asphalt: return ShaderSurface::Architecture;
    case TextureMaterial::Water: return ShaderSurface::Water;
    case TextureMaterial::None: return ShaderSurface::Shadow;
    default: return ShaderSurface::Terrain;
    }
}
// External editable shader source package. Reading/validation has no D3D dependency.
// D3D renderer compiles sources and commits a complete pipeline transactionally.
class ShaderLibrary {
public:
    bool load(const std::filesystem::path& root="assets/shaders");
    const std::string& scene() const {return scene_;}
    const std::string& overlay() const {return overlay_;}
    const std::string& error() const {return error_;}
    bool valid() const {return !scene_.empty() && !overlay_.empty();}
private:
    std::string scene_,overlay_,error_;
};
}
