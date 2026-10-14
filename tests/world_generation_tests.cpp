#include "gz/TerrainAppearance.hpp"
#include "gz/Terrain.hpp"
#include "gz/Campaign.hpp"
#include "TestCheck.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <thread>
#include <iostream>
static std::uint32_t crc(const std::vector<std::uint8_t>& b){std::uint32_t c=~0u;for(auto v:b){c^=v;for(int i=0;i<8;++i)c=(c>>1)^(0xedb88320u&-(c&1u));}return ~c;}
static void put(std::vector<std::uint8_t>& b,std::size_t i,std::uint32_t x){for(int j=0;j<4;++j)b[i+j]=std::uint8_t(x>>(j*8));}
static std::uint32_t get(const std::vector<std::uint8_t>& b,std::size_t i){std::uint32_t x=0;for(int j=0;j<4;++j)x|=std::uint32_t(b[i+j])<<(j*8);return x;}
int main(){
    gz::WorldSettings settings{123456789,1,gz::TerrainRegion::Forest};
    gz::HeightMap a,b;GZ_CHECK(a.generate({0,0},settings)&&b.generate({1,0},settings));
    for(int z=0;z<gz::kTerrainSamples;++z)GZ_CHECK(a.pixel(1024,z)==b.pixel(0,z));
    GZ_CHECK(gz::TerrainGenerator::height(2800,1400,{})==gz::proceduralElevation(2800,1400));
    auto changed=settings;changed.seed++;
    GZ_CHECK(gz::TerrainGenerator::height(2800,1400,settings)!=gz::TerrainGenerator::height(2800,1400,changed));
    const auto temp=std::filesystem::temp_directory_path()/"grayzone_world_v024_test";std::filesystem::create_directories(temp);
    gz::TerrainWorld world(2);world.setGenerator(settings);world.setGeneratedCacheDirectory(temp.string());
    const float analytic=world.heightAt(2782.13,1230.5);
    GZ_CHECK(std::abs(analytic-a.sample(2782.13f,1230.5f))<.001f);
    GZ_CHECK(world.pendingSectorCount()<=1);
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
    while(world.pendingSectorCount()&&std::chrono::steady_clock::now()<deadline){world.pollGenerated();std::this_thread::sleep_for(std::chrono::milliseconds(5));}
    GZ_CHECK(world.pendingSectorCount()==0 && world.cachedSectorCount()==1);
    GZ_CHECK(std::abs(world.heightAt(2782.13,1230.5)-analytic)<.001f);
    auto macro=gz::TerrainAppearance::generate(settings,0,0,128);
    auto same=gz::TerrainAppearance::generate(settings,0,0,128);
    GZ_CHECK(macro&&macro->view().valid()&&macro->pixels==same->pixels);
    GZ_CHECK(macro->pixels.size()==87380); // Full RGBA mip chain 128 down to 1.
    GZ_CHECK(gz::TerrainAppearance::generate(changed,0,0,128)->pixels!=macro->pixels);
    GZ_CHECK(!gz::TerrainAppearance::generate(settings,0,0,300));
    gz::ScenarioCatalog catalog;GZ_CHECK(catalog.loadDirectory("assets/scenarios"));
    gz::Simulation sim(1);sim.newOperation(48);gz::Campaign campaign;campaign.begin(*catalog.at(0),sim);campaign.setWorldSettings(settings);
    auto path=temp/"campaign.sav";GZ_CHECK(campaign.saveSlot(path.string(),sim));
    gz::Campaign restored;gz::Simulation loaded(1);GZ_CHECK(restored.loadSlot(path.string(),loaded,catalog));GZ_CHECK(restored.state().world==settings);
    std::ifstream in(path,std::ios::binary);std::vector<std::uint8_t> bytes(std::istreambuf_iterator<char>{in},{});in.close();
    const auto metadata=get(bytes,9);bytes.erase(bytes.begin()+21+metadata-16,bytes.begin()+21+metadata);
    put(bytes,5,2);put(bytes,9,metadata-16);
    std::vector<std::uint8_t> body(bytes.begin()+21,bytes.end());put(bytes,17,crc(body));
    std::ofstream old(path,std::ios::binary|std::ios::trunc);old.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));old.close();
    GZ_CHECK(restored.loadSlot(path.string(),loaded,catalog)&&restored.state().world.version==0);
    std::filesystem::remove_all(temp);
    std::cout<<"Seeded terrain seams, async heightmap consistency, macro mips and v2/v3 campaign migration: PASS\n";
}
