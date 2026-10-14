#pragma once
#include "gz/NativeScene.hpp"
#include "gz/SceneLayout.hpp"
#include <array>
namespace gz {
enum class SpriteKind:std::uint8_t {Broadleaf,Pine,DryTree,Grass,Reed,Flowers,Fern,Cloud,Muzzle,Smoke,Spark,Dust,Impact,Rain,Leaf,Fire,Count};
struct SpriteVertex {float x,y,z,u,v,r,g,b,a,layer;};
static_assert(sizeof(SpriteVertex)==40);
struct Sprite {float x=0,y=0,z=0,width=1,height=1;SpriteKind kind=SpriteKind::Grass;std::array<float,4> tint{1,1,1,1};bool upright=true;float axisX=0,axisY=0,axisZ=0;};
class SpriteAtlas {
public:
    std::uint32_t resolution=0,layers=0;std::vector<std::uint8_t> pixels;
    bool load(const std::string& path="assets/sprites/procedural.gzsa");
    std::array<float,4> sample(SpriteKind kind,float u,float v)const;
};
class SpriteBatch {
    std::vector<Sprite> sprites_;
    std::vector<SpriteVertex> vertices_;
    std::vector<std::uint32_t> indices_;
    std::uint64_t revision_=0;
public:
    void clear(){sprites_.clear();vertices_.clear();indices_.clear();}
    bool add(Sprite sprite);
    void finish(const Camera& camera,float ground=0);
    void build(TerrainWorld& world,const Simulation& sim,const Camera& camera,AtmosphereSettings atmosphere,float visualTime,int quality=2,bool wind=true);
    GeometryView geometry()const{return {vertices_.data(),vertices_.size()*sizeof(SpriteVertex),sizeof(SpriteVertex),indices_.data(),indices_.size(),0,0,0,0,revision_};}
    std::size_t count()const{return sprites_.size();}
    const std::vector<Sprite>& sortedSprites()const{return sprites_;}
};
}
