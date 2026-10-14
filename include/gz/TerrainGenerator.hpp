#pragma once
#include <cstdint>
namespace gz {
enum class TerrainRegion : std::uint32_t {Farmland,Forest,Wetland,Count};
struct WorldSettings {
    std::uint64_t seed=1989;
    std::uint32_t version=0; // 0 preserves the original authored/imported world.
    TerrainRegion region=TerrainRegion::Farmland;
    bool operator==(const WorldSettings&)const=default;
    bool valid()const{return version<=1&&std::uint32_t(region)<std::uint32_t(TerrainRegion::Count);}
};
struct TerrainSurface {float grass=1,soil=0,rock=0,wetness=0,road=0,water=0;};
class TerrainGenerator {
public:
    static float height(double x,double z,const WorldSettings& settings);
    static float riverCenter(double z,const WorldSettings& settings);
    static float riverWidth(double z,const WorldSettings& settings);
    static TerrainSurface surface(double x,double z,float height,float slope,const WorldSettings& settings);
};
}
