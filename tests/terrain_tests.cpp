#include "gz/Terrain.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
using namespace gz;
static bool closeEnough(float a,float b,float eps=0.001f){return std::abs(a-b)<=eps;}
int main() {
    HeightMap a,b,c;
    if(!a.generate({0,0}) || !b.generate({1,0}) || !c.generate({0,-1}))return 1;
    if(a.sampleCount()!=1025u*1025u || a.bytes()!=1025u*1025u*2u)return 2;
    // Left/right, top/bottom adjacent sectors share exactly the same signed 16-bit pixels.
    for(int i=0;i<kTerrainSamples;++i){
        if(a.pixel(kTerrainSamples-1,i)!=b.pixel(0,i))return 3;
        if(a.pixel(i,0)!=c.pixel(i,kTerrainSamples-1))return 4;
    }
    const auto scratch=std::filesystem::temp_directory_path()/"gz_terrain_test_1989.gzt";
    const auto pgm=std::filesystem::temp_directory_path()/"gz_terrain_test_1989.pgm";
    if(!a.saveBinary(scratch.string()) || !a.savePGM16(pgm.string()))return 5;
    HeightMap fromPixels;
    if(!fromPixels.loadPGM16(pgm.string(),{0,0}))return 16;
    if(a.pixel(74,800)!=fromPixels.pixel(74,800))return 17;
    HeightMap loaded;
    if(!loaded.loadBinary(scratch.string()) || loaded.sector()!=TerrainSectorId{0,0})return 6;
    for(int i=0;i<kTerrainSamples;i+=17)for(int j=0;j<kTerrainSamples;j+=17)
        if(a.pixel(i,j)!=loaded.pixel(i,j))return 7;
    if(!closeEnough(a.sample(502.75f,1201.5f),loaded.sample(502.75f,1201.5f)))return 8;
    // A 16-bit PGM contains 2 bytes per pixel plus a short text header.
    const auto pgmBytes=std::filesystem::file_size(pgm);
    if(pgmBytes<kTerrainSamples*kTerrainSamples*2u ||
       pgmBytes>kTerrainSamples*kTerrainSamples*2u+512u)return 9;
    // Corruption must be rejected rather than silently changing mountains.
    {
      std::fstream f(scratch,std::ios::in|std::ios::out|std::ios::binary);
      f.seekp(30,std::ios::beg);f.put(char(0xff));
    }
    HeightMap bad;
    if(bad.loadBinary(scratch.string()))return 10;
    // Edited pixel image tiles can override the procedural sector generator.
    const auto scratchDir=std::filesystem::temp_directory_path()/"gz_sector_images_test";
    std::filesystem::create_directories(scratchDir);
    const auto imageTile=scratchDir/"0_0.pgm";
    a.savePGM16(imageTile.string());
    TerrainWorld importedWorld(2);importedWorld.setTileDirectory(scratchDir.string());
    if(!closeEnough(importedWorld.heightAt(500,600),a.sample(500,600))||importedWorld.diskLoads()!=1)return 18;
    std::filesystem::remove(imageTile);std::filesystem::remove(scratchDir);
    TerrainWorld world(2);
    auto pos=world.sectorAt(-0.5,-3000.1);
    if(pos.x!=-1 || pos.z!=-2)return 11;
    const float h0=world.heightAt(2999.9,1250),h1=world.heightAt(3000.1,1250);
    if(std::abs(h0-h1)>2.f)return 12;
    world.heightAt(-700,1250);
    world.heightAt(7000,1250);
    if(world.cachedSectorCount()!=2 || world.cachedBytes()>2u*1025u*1025u*2u)return 13;
    if(world.sectorLoads()<3)return 14;
    if(!std::isfinite(world.slopeAt(3200,1300)))return 15;
    std::filesystem::remove(scratch);std::filesystem::remove(pgm);
    std::cout<<"TERRAIN PASS: 16-bit pixels, matching seams, binary roundtrip, corruption detection, LRU, negative coordinates, slopes\n";
    return 0;
}
