#include "gz/TerrainGenerator.hpp"
#include "gz/Terrain.hpp"
#include "gz/LandscapeVisual.hpp"
#include <algorithm>
#include <cmath>
namespace gz {
namespace {
float seededNoise(double x,double z,double period,std::uint64_t seed){
    const double dx=double(seed&65535u)*.731,dz=double((seed>>16)&65535u)*.937;
    return landscapeNoise(float(x+dx),float(z+dz),float(period));
}
}
float TerrainGenerator::riverCenter(double z,const WorldSettings& s){
    if(!s.version)return 3350.f+95.f*std::sin(float(z*.0025));
    const float shift=float(s.seed%151)-75.f;
    return 3350.f+shift+95.f*std::sin(float(z*.0025))+32.f*(seededNoise(0,z,900,s.seed+89)-.5f);
}
float TerrainGenerator::riverWidth(double z,const WorldSettings& s){
    return (s.region==TerrainRegion::Wetland&&s.version?44.f:26.f)+4.f*std::sin(float(z*.0041));
}
float TerrainGenerator::height(double x,double z,const WorldSettings& s){
    if(!s.version)return proceduralElevation(x,z);
    const float warp=seededNoise(x,z,1800,s.seed)*110.f-55.f;
    const float scale=s.region==TerrainRegion::Forest?48.f:s.region==TerrainRegion::Wetland?8.f:24.f;
    const float large=seededNoise(x+warp,z-warp,950,s.seed)-.5f;
    const float medium=seededNoise(x,z,230,s.seed+211)-.5f;
    const float small=seededNoise(x,z,41,s.seed+19)-.5f;
    float ground=10.f+scale*large+8.f*medium+1.2f*small;
    const float bank=std::clamp((std::abs(float(x)-riverCenter(z,s))-riverWidth(z,s)+4.f)/55.f,0.f,1.f);
    ground=(5.f+std::sin(float(z*.00065)))*(1.f-bank*bank*(3-2*bank))+ground*bank*bank*(3-2*bank);
    // Shared deterministic road grades keep the initial village and mission corridors usable.
    const float road=std::min(std::abs(float(z)-roadCenterHorizontal(float(x))),std::abs(float(x)-roadCenterVertical(float(z))));
    const float grade=std::clamp((road-12.f)/26.f,0.f,1.f);
    const float roadHeight=8.f+scale*.24f*large;
    return roadHeight*(1-grade)+ground*grade;
}
TerrainSurface TerrainGenerator::surface(double x,double z,float h,float slope,const WorldSettings& s){
    const float vegetation=seededNoise(x,z,180,s.seed+51);
    const float bank=std::abs(float(x)-riverCenter(z,s))-riverWidth(z,s);
    const float roadDistance=std::min(std::abs(float(z)-roadCenterHorizontal(float(x))),std::abs(float(x)-roadCenterVertical(float(z))));
    TerrainSurface out;
    out.water=std::clamp(-bank/6.f,0.f,1.f);
    out.wetness=std::clamp(1.f-std::max(bank,0.f)/50.f,0.f,1.f)*(s.region==TerrainRegion::Wetland?1.f:.8f);
    out.road=1.f-std::clamp((roadDistance-10.f)/5.f,0.f,1.f);
    out.rock=std::clamp((slope-.35f)*2.2f,0.f,1.f);
    out.soil=std::clamp((vegetation-.55f)*2.2f+out.wetness*.42f+std::max(0.f,h-35.f)*.005f,0.f,1.f);
    out.grass=std::clamp(1.f-out.soil-out.rock,0.f,1.f);
    return out;
}
}
