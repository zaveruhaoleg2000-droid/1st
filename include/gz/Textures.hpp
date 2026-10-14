#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace gz {
// IDs are stable and correspond to assets/textures/manifest.json.
enum class TextureMaterial : std::uint8_t {
    None, GrassShort, GrassTall, Meadow, DirtDry, MudWet, Sand, Gravel,
    Rock, Asphalt, Concrete, Brick, WoodPlanks, TreeBark, Leaves, Water, Snow, PlayerUniform, OpponentUniform, Skin, Armor, Rubber, Glass, Roof, WeaponMetal, WeaponWood, Fabric, Stone, RoofSlate, LeavesDry, Fence, Crate, Rubble, Moss, ReedStem, Wildflowers, PineBark, Plaster, Rust, CeramicTile, Count
};
class TextureLibrary {
public:
    // Read only one resolution per material. All other mip sizes stay on disk.
    // The GZT1 RGB8 format needs no PNG/zlib dependency in the CPU renderer.
    std::size_t load(const std::string& directory, int resolution = 256);
    std::uint32_t sample(TextureMaterial material, float u, float v) const;
    bool has(TextureMaterial material) const;
    std::size_t loadedCount() const { return loadedCount_; }
    std::size_t bytes() const { return memoryBytes_; }
    int resolution() const { return resolution_; }
    static const char* name(TextureMaterial material);
    static float metersPerRepeat(TextureMaterial material);
private:
    struct Image { int w = 0, h = 0; std::vector<std::uint32_t> pixels; };
    std::array<Image, static_cast<std::size_t>(TextureMaterial::Count)> images_{};
    std::size_t loadedCount_ = 0, memoryBytes_ = 0;
    int resolution_ = 0;
};
}
