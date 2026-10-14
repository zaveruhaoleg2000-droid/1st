#include "gz/Terrain.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <utility>

namespace gz {
namespace {
constexpr std::size_t kPixelCount = std::size_t(kTerrainSamples)*kTerrainSamples;
float noiseHash(int x,int z) {
    std::uint32_t h=std::uint32_t(x)*374761393u+std::uint32_t(z)*668265263u+0x9e3779b9u;
    h=(h^(h>>13))*1274126177u;
    return float((h^(h>>16))&65535u)/65535.f;
}
float smooth(float a,float b,float t) { t=t*t*(3.f-2.f*t); return a+(b-a)*t; }
float noise(double x,double z,double period) {
    double fx=x/period, fz=z/period;
    int ix=int(std::floor(fx)), iz=int(std::floor(fz));
    float tx=float(fx-ix), tz=float(fz-iz);
    return smooth(smooth(noiseHash(ix,iz),noiseHash(ix+1,iz),tx),
                  smooth(noiseHash(ix,iz+1),noiseHash(ix+1,iz+1),tx),tz);
}
float riverCenter(double z) { return 3350.f+95.f*std::sin(float(z*.0025)); }
float riverWidth(double z) { return 26.f+4.f*std::sin(float(z*.0041)); }
float riverLevel(double z) { return 7.f+1.2f*std::sin(float(z*.00065)); }
void writeU32(std::ostream& s,std::uint32_t x) {
    for(int i=0;i<4;++i)s.put(char((x>>(i*8))&255u));
}
bool readU32(std::istream& s,std::uint32_t& v) {
    v=0;
    for(int i=0;i<4;++i) { const int c=s.get(); if(c<0)return false;v|=std::uint32_t(c)<<(8*i); }
    return true;
}
std::uint32_t checksum(const std::vector<std::int16_t>& values) {
    std::uint32_t h=2166136261u;
    for(auto x:values){const auto v=std::uint16_t(x);h=(h^std::uint8_t(v))*16777619u;h=(h^std::uint8_t(v>>8))*16777619u;}
    return h;
}
}
float proceduralElevation(double x,double z) {
    const float base=6.f+18.f*(noise(x,z,430.)-.5f)
                       +7.f*(noise(x,z,110.)-.5f)
                       +2.f*(noise(x,z,36.)-.5f);
    const float d=std::abs(float(x)-riverCenter(z));
    float bank=std::clamp((d-riverWidth(z)+4.f)/50.f,0.f,1.f);
    bank=bank*bank*(3.f-2.f*bank);
    return (riverLevel(z)-2.f)*(1.f-bank)+base*bank;
}
bool HeightMap::generate(TerrainSectorId sector) {
    // Preserve the reference procedural function and its 0.1 m quantization.
    // Instead of recomputing floor(), 12 hashes and 3 river sine waves for
    // every pixel, cache X interpolation positions and each lattice cell's
    // four corner hashes. River parameters depend only on the row (Z).
    struct XCell { int lattice; float t; };
    constexpr std::array<double,3> periods{430.,110.,36.};
    std::array<std::array<XCell,kTerrainSamples>,3> cols{};
    const double ox=double(sector.x)*kTerrainSectorSize;
    const double oz=double(sector.z)*kTerrainSectorSize;
    for(int octave=0;octave<3;++octave){
        for(int x=0;x<kTerrainSamples;++x){
            const double wx=ox+double(x)*kTerrainSectorSize/kTerrainCells;
            const double fx=wx/periods[octave];
            const int ix=int(std::floor(fx));
            cols[octave][x]={ix,float(fx-ix)};
        }
    }
    sector_=sector;
    values_.resize(kPixelCount);
    for(int z=0;z<kTerrainSamples;++z){
        const double wz=oz+double(z)*kTerrainSectorSize/kTerrainCells;
        const float center=riverCenter(wz);
        const float width=riverWidth(wz);
        const float level=riverLevel(wz);
        struct OctaveRow {
            int iz, previousIx=0;
            float tz, h00=0,h10=0,h01=0,h11=0;
            bool valid=false;
        };
        std::array<OctaveRow,3> rows{};
        for(int octave=0;octave<3;++octave){
            const double fz=wz/periods[octave];
            rows[octave].iz=int(std::floor(fz));
            rows[octave].tz=float(fz-rows[octave].iz);
        }
        for(int x=0;x<kTerrainSamples;++x){
            float n[3];
            for(int octave=0;octave<3;++octave){
                auto& row=rows[octave];
                const auto cell=cols[octave][x];
                if(!row.valid || row.previousIx!=cell.lattice){
                    row.previousIx=cell.lattice;row.valid=true;
                    row.h00=noiseHash(cell.lattice,row.iz);
                    row.h10=noiseHash(cell.lattice+1,row.iz);
                    row.h01=noiseHash(cell.lattice,row.iz+1);
                    row.h11=noiseHash(cell.lattice+1,row.iz+1);
                }
                n[octave]=smooth(smooth(row.h00,row.h10,cell.t),
                                 smooth(row.h01,row.h11,cell.t),row.tz);
            }
            const double wx=ox+double(x)*kTerrainSectorSize/kTerrainCells;
            const float base=6.f+18.f*(n[0]-.5f)+7.f*(n[1]-.5f)+2.f*(n[2]-.5f);
            const float d=std::abs(float(wx)-center);
            float bank=std::clamp((d-width+4.f)/50.f,0.f,1.f);
            bank=bank*bank*(3.f-2.f*bank);
            const float elevation=(level-2.f)*(1.f-bank)+base*bank;
            const long dm=std::lround(double(elevation)*10.0);
            values_[std::size_t(z)*kTerrainSamples+x]=std::int16_t(std::clamp(dm,-32768L,32767L));
        }
    }
    return true;
}

std::int16_t HeightMap::pixel(std::size_t x,std::size_t z) const {
    if(x>=kTerrainSamples||z>=kTerrainSamples||values_.size()!=kPixelCount)throw std::out_of_range("Terrain pixel");
    return values_[z*kTerrainSamples+x];
}
float HeightMap::sample(float globalX,float globalZ) const {
    if(values_.size()!=kPixelCount)return 0.f;
    const float xf=std::clamp(float((double(globalX)-double(sector_.x)*kTerrainSectorSize)/kTerrainCellSize),0.f,float(kTerrainCells));
    const float zf=std::clamp(float((double(globalZ)-double(sector_.z)*kTerrainSectorSize)/kTerrainCellSize),0.f,float(kTerrainCells));
    const std::size_t ix=std::min(std::size_t(xf),std::size_t(kTerrainCells-1)), iz=std::min(std::size_t(zf),std::size_t(kTerrainCells-1));
    const float dx=xf-float(ix),dz=zf-float(iz);
    const std::size_t p=iz*kTerrainSamples+ix;
    const float a=float(values_[p])*(1.f-dx)+float(values_[p+1])*dx;
    const float b=float(values_[p+kTerrainSamples])*(1.f-dx)+float(values_[p+kTerrainSamples+1])*dx;
    return (a*(1.f-dz)+b*dz)*0.1f;
}
bool HeightMap::saveBinary(const std::string& path) const {
    if(values_.size()!=kPixelCount)return false;
    std::ofstream out(path,std::ios::binary);
    if(!out)return false;
    out.write("GZHMAP01",8);
    writeU32(out,1);
    writeU32(out,std::uint32_t(sector_.x));writeU32(out,std::uint32_t(sector_.z));
    writeU32(out,kTerrainSamples);writeU32(out,checksum(values_));
    if constexpr(std::endian::native==std::endian::little){
        out.write(reinterpret_cast<const char*>(values_.data()),
                  std::streamsize(values_.size()*sizeof(std::int16_t)));
    }else{
        for(auto n:values_){const auto v=std::uint16_t(n);out.put(char(v&255u));out.put(char(v>>8));}
    }
    return bool(out);
}
bool HeightMap::loadBinary(const std::string& path) {
    std::ifstream in(path,std::ios::binary);
    if(!in)return false;
    char magic[8];if(!in.read(magic,8)||std::string(magic,8)!="GZHMAP01")return false;
    std::uint32_t version=0,x=0,z=0,dimension=0,crc=0;
    if(!readU32(in,version)||!readU32(in,x)||!readU32(in,z)||!readU32(in,dimension)||!readU32(in,crc))return false;
    if(version!=1||dimension!=kTerrainSamples)return false;
    std::vector<std::int16_t> buf(kPixelCount);
    if constexpr(std::endian::native==std::endian::little){
        if(!in.read(reinterpret_cast<char*>(buf.data()),
                    std::streamsize(buf.size()*sizeof(std::int16_t))))return false;
    }else{
        for(auto& v:buf){int lo=in.get(),hi=in.get();if(lo<0||hi<0)return false;
            v=std::int16_t(std::uint16_t(lo)|(std::uint16_t(hi)<<8));}
    }
    // Reject trailing garbage and detect truncated/corrupted tile files.
    if(in.peek()!=std::char_traits<char>::eof()||checksum(buf)!=crc)return false;
    sector_={std::int32_t(x),std::int32_t(z)};
    values_=std::move(buf);
    return true;
}
bool HeightMap::savePGM16(const std::string& path) const {
    if(values_.size()!=kPixelCount)return false;
    std::ofstream out(path,std::ios::binary);
    if(!out)return false;
    out<<"P5\n# Gray Zone terrain: unsigned pixels encode signed heights in decimeters, offset +32768\n"
       <<kTerrainSamples<<" "<<kTerrainSamples<<"\n65535\n";
    for(auto h:values_) {
        // PGM stores MSB first; use a fixed offset for signed decimeters.
        std::uint16_t v=std::uint16_t(std::int32_t(h)+32768);
        out.put(char(v>>8));out.put(char(v&255));
    }
    return bool(out);
}
bool HeightMap::loadPGM16(const std::string& path,TerrainSectorId sector) {
    std::ifstream in(path,std::ios::binary);
    if(!in)return false;
    // Standard P5 grayscale. Comment lines are allowed; values are big-endian.
    auto token=[&]()->std::string {
        std::string out;
        while(true){
            int ch=in.peek();
            if(ch<0)return {};
            if(ch=='#'){in.ignore(std::numeric_limits<std::streamsize>::max(),'\n');continue;}
            if(ch==' '||ch=='\n'||ch=='\r'||ch=='\t'){in.get();continue;}
            break;
        }
        while(true){
            int ch=in.peek();
            if(ch<0 || ch==' ' || ch=='\n' || ch=='\r' || ch=='\t' || ch=='#')break;
            if(out.size()>20)return {};
            out.push_back(char(in.get()));
        }
        return out;
    };
    if(token()!="P5"||token()!="1025"||token()!="1025"||token()!="65535")return false;
    // Exactly one whitespace character is the separator before binary data.
    int separator=in.get();
    if(separator!=' '&&separator!='\r'&&separator!='\n'&&separator!='\t')return false;
    // Bulk I/O is substantially quicker than 2M individual stream::get calls.
    std::vector<std::uint8_t> raw(kPixelCount*2);
    if(!in.read(reinterpret_cast<char*>(raw.data()),std::streamsize(raw.size())))return false;
    if(in.peek()!=std::char_traits<char>::eof())return false;
    std::vector<std::int16_t> tmp(kPixelCount);
    for(std::size_t i=0;i<kPixelCount;++i){
        const auto biased=std::uint16_t((std::uint16_t(raw[2*i])<<8)|std::uint16_t(raw[2*i+1]));
        tmp[i]=std::int16_t(int(biased)-32768);
    }
    values_=std::move(tmp); sector_=sector;
    return true;
}
TerrainWorld::TerrainWorld(std::size_t maxCachedSectors):maxCached_(std::clamp(maxCachedSectors,std::size_t(1),std::size_t(16))){}
TerrainSectorId TerrainWorld::sectorAt(double x,double z) const {
    return {std::int32_t(std::floor(x/kTerrainSectorSize)),std::int32_t(std::floor(z/kTerrainSectorSize))};
}
HeightMap& TerrainWorld::acquire(TerrainSectorId id) {
    for(auto it=cache_.begin();it!=cache_.end();++it){if(it->id==id){cache_.splice(cache_.begin(),cache_,it);return cache_.front().map;}}
    // Bound resident memory before allocating the next tile.
    if(cache_.size()>=maxCached_)cache_.pop_back();
    cache_.emplace_front();cache_.front().id=id;
    bool loaded=false;
    if(!directory_.empty()){
        const auto name=std::to_string(id.x)+"_"+std::to_string(id.z);
        const auto image=(std::filesystem::path(directory_)/(name+".pgm")).string();
        const auto packed=(std::filesystem::path(directory_)/(name+".gzt")).string();
        if(std::filesystem::exists(image))loaded=cache_.front().map.loadPGM16(image,id);
        else if(std::filesystem::exists(packed)){
            loaded=cache_.front().map.loadBinary(packed);
            loaded=loaded && cache_.front().map.sector()==id;
        }
    }
    if(loaded)++diskLoads_;
    else{ cache_.front().map.generate(id);++generationCount_; }
    return cache_.front().map;
}
float TerrainWorld::heightAt(double x,double z) {return acquire(sectorAt(x,z)).sample(float(x),float(z));}
float TerrainWorld::slopeAt(double x,double z) {
    constexpr double step=6.;
    const float dx=(heightAt(x+step,z)-heightAt(x-step,z))/(2*float(step));
    const float dz=(heightAt(x,z+step)-heightAt(x,z-step))/(2*float(step));
    return std::atan(std::sqrt(dx*dx+dz*dz)); // radians
}
bool TerrainWorld::exportSector(TerrainSectorId id,const std::string& path,bool pgm) {
    auto& m=acquire(id);
    return pgm?m.savePGM16(path):m.saveBinary(path);
}
void TerrainWorld::setTileDirectory(std::string directory){directory_=std::move(directory);cache_.clear();}
void TerrainWorld::setMaxCachedSectors(std::size_t n){maxCached_=std::clamp(n,std::size_t(1),std::size_t(16));while(cache_.size()>maxCached_)cache_.pop_back();}
std::size_t TerrainWorld::cachedBytes() const {std::size_t b=0;for(const auto& e:cache_)b+=e.map.bytes();return b;}
}
