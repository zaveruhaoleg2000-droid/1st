#include "gz/Textures.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <vector>

namespace gz {
namespace {
constexpr const char* NAMES[] = {
    "none", "grass_short", "grass_tall", "meadow", "dirt_dry", "mud_wet",
    "sand", "gravel", "rock", "asphalt", "concrete", "brick", "wood_planks",
    "tree_bark", "leaves", "water", "snow", "player_uniform", "opponent_uniform", "skin", "armor", "rubber", "glass", "roof", "weapon_metal", "weapon_wood", "fabric", "stone", "roof_slate", "leaves_dry", "fence", "crate", "rubble", "moss", "reed_stem", "wildflowers", "pine_bark", "plaster", "rust", "ceramic_tile"
};
constexpr float TILES[] = { 1.f,4.f,5.f,3.5f,4.f,3.f,3.f,4.f,4.f,3.f,2.f,4.f,3.f,3.f,3.f,1.5f,1.8f,.35f,.35f,.3f,1.f,.6f,1.f,3.f,.3f,.4f,.3f,2.f,3.f,2.f,1.f,1.f,2.f,1.f,.4f,1.f,2.f,2.f,1.f,1.5f };
constexpr std::size_t COUNT=sizeof(NAMES)/sizeof(NAMES[0]);
static_assert(COUNT==static_cast<std::size_t>(TextureMaterial::Count));
}
const char* TextureLibrary::name(TextureMaterial m) {
    const auto i=static_cast<std::size_t>(m);return i<COUNT?NAMES[i]:"none";
}
float TextureLibrary::metersPerRepeat(TextureMaterial m){
    const auto i=static_cast<std::size_t>(m);return i<COUNT?TILES[i]:1.f;
}
bool TextureLibrary::has(TextureMaterial m) const {
    const auto i=static_cast<std::size_t>(m);return i<images_.size()&&!images_[i].pixels.empty();
}
std::size_t TextureLibrary::load(const std::string& directory, int resolution){
    for(auto& image:images_){image=Image{};}
    loadedCount_=memoryBytes_=0;
    resolution_ = (resolution==128 || resolution==256 || resolution==512)?resolution:256;
    for(std::size_t i=1;i<COUNT;++i){
        auto path=std::filesystem::path(directory)/(std::string(NAMES[i])+"_"+std::to_string(resolution_)+".gzt");
        std::ifstream in(path,std::ios::binary);
        if(!in)continue;
        unsigned char head[8]{};in.read(reinterpret_cast<char*>(head),8);
        if(!in || head[0]!='G'||head[1]!='Z'||head[2]!='T'||head[3]!='1')continue;
        const int w=int(head[4])|int(head[5])<<8,h=int(head[6])|int(head[7])<<8;
        if(w!=resolution_ || h!=resolution_)continue;
        std::vector<std::uint8_t> bytes(std::size_t(w)*std::size_t(h)*3u);
        in.read(reinterpret_cast<char*>(bytes.data()),std::streamsize(bytes.size()));
        if(!in)continue;  // Reject corrupt/partial resource, do not sample it.
        Image image;image.w=w;image.h=h;image.pixels.resize(std::size_t(w)*h);
        for(std::size_t j=0;j<image.pixels.size();++j)
            image.pixels[j]=(std::uint32_t(bytes[j*3])<<16)|(std::uint32_t(bytes[j*3+1])<<8)|bytes[j*3+2];
        memoryBytes_+=image.pixels.size()*sizeof(std::uint32_t);++loadedCount_;
        images_[i]=std::move(image);
    }
    return loadedCount_;
}
std::uint32_t TextureLibrary::sample(TextureMaterial m,float u,float v) const {
    const auto index=static_cast<std::size_t>(m);
    if(index>=images_.size())return 0;
    const auto& img=images_[index];
    if(img.pixels.empty()||!std::isfinite(u)||!std::isfinite(v))return 0;
    // Toroidal coordinates: negative world positions and neighboring chunks tile seamlessly.
    const float uf=u-std::floor(u),vf=v-std::floor(v);
    const int x=std::min(img.w-1,int(uf*float(img.w))),y=std::min(img.h-1,int(vf*float(img.h)));
    return img.pixels[std::size_t(y)*img.w+x];
}
}
