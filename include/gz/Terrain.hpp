#pragma once
#include <cstddef>
#include <cstdint>
#include <list>
#include <string>
#include <vector>

namespace gz {
// A 3km x 3km world sector is a 1024x1024 grid of cells (1025x1025
// image pixels/vertices, including both edges). Height is stored in 0.1m.
constexpr int kTerrainCells = 1024;
constexpr int kTerrainSamples = kTerrainCells + 1;
constexpr double kTerrainSectorSize = 3000.0;
constexpr float kTerrainCellSize = float(kTerrainSectorSize / kTerrainCells);
constexpr int kTerrainChunksPerSector = 16;
constexpr float kTerrainChunkSize = float(kTerrainSectorSize / kTerrainChunksPerSector);

struct TerrainSectorId {
    std::int32_t x = 0, z = 0;
    bool operator==(const TerrainSectorId&) const = default;
};

class HeightMap final {
public:
    bool generate(TerrainSectorId sector);
    bool saveBinary(const std::string& path) const;
    bool loadBinary(const std::string& path);
    bool loadPGM16(const std::string& path, TerrainSectorId sector);
    bool savePGM16(const std::string& path) const;
    float sample(float globalX, float globalZ) const;
    std::int16_t pixel(std::size_t x, std::size_t z) const;
    TerrainSectorId sector() const { return sector_; }
    std::size_t bytes() const { return values_.size() * sizeof(std::int16_t); }
    std::size_t sampleCount() const { return values_.size(); }
private:
    TerrainSectorId sector_{};
    std::vector<std::int16_t> values_;
};

// Deterministic height function provides identical samples on shared tile edges.
float proceduralElevation(double x, double z);

class TerrainWorld final {
public:
    explicit TerrainWorld(std::size_t maxCachedSectors=4);
    float heightAt(double x, double z);
    float slopeAt(double x, double z);
    bool exportSector(TerrainSectorId id,const std::string& path,bool pgm=false);
    TerrainSectorId sectorAt(double x,double z) const;
    std::size_t cachedSectorCount() const { return cache_.size(); }
    std::size_t cachedBytes() const;
    std::uint64_t sectorLoads() const { return generationCount_; }
    void setMaxCachedSectors(std::size_t n);
    void setTileDirectory(std::string directory);
    std::uint64_t diskLoads() const { return diskLoads_; }
    std::uint64_t contentRevision()const{return contentRevision_;}
private:
    HeightMap& acquire(TerrainSectorId id);
    struct CacheEntry { TerrainSectorId id; HeightMap map; };
    std::list<CacheEntry> cache_; // MRU at front
    std::size_t maxCached_ = 4;
    std::uint64_t generationCount_ = 0;
    std::uint64_t diskLoads_ = 0;
    std::uint64_t contentRevision_ = 0;
    std::string directory_;
};
}
