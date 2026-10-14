#pragma once
#include "gz/Simulation.hpp"
#include "gz/Terrain.hpp"
#include "gz/Europe.hpp"
#include "gz/Textures.hpp"
#include "gz/Atmosphere.hpp"
#include "gz/EnvironmentFX.hpp"
#include "gz/Shaders.hpp"
#include <array>
#include <optional>
#include <cstdint>
#include <string>
#include <vector>
namespace gz {
struct Vec3 {float x=0,y=0,z=0;};
struct Camera {
    float focusX=3000,focusZ=1250;
    float yaw=0.34f,pitch=0.82f,distance=1950;
};
struct VisualEffects {
    bool antialiasing = true;      // Low-cost, edge-adaptive post filter (one 32-bit buffer).
    bool wind = true;              // Deterministic canopy sway, keyed by visual time.
    bool waterReflections = true;  // Procedural sky/riverbank reflection approximation.
};
class Renderer {
public:
    void resize(int w,int h);
    std::size_t loadTextures(const std::string& directory="assets/textures/runtime",int resolution=256){return textures_.load(directory,resolution);}
    void setTexturing(bool enabled){texturingEnabled_=enabled;}
    bool texturingEnabled()const{return texturingEnabled_;}
    std::size_t textureMemoryBytes()const{return textures_.bytes();}
    std::size_t textureCount()const{return textures_.loadedCount();}
    void setEffects(VisualEffects effects) { effects_ = effects; }
    VisualEffects effects() const { return effects_; }
    void setShaderQuality(ShaderQuality quality){shaderQuality_=quality;}
    ShaderQuality shaderQuality()const{return shaderQuality_;}
    void setDynamicLights(bool enabled){dynamicLights_=enabled;}
    bool dynamicLights()const{return dynamicLights_;}
    void setPrecipitation(bool enabled){precipitation_=enabled;}
    bool precipitation()const{return precipitation_;}
    void triggerExplosion(float x,float z,float atTime){explosion_={x,z,atTime};}
    ExplosionEvent explosion()const{return explosion_;}
    void setVisualTime(float seconds) { visualTime_ = seconds; }
    float visualTime() const { return visualTime_; }
    void setAtmosphere(AtmosphereSettings a){ atmosphere_=a; }
    AtmosphereSettings atmosphere() const { return atmosphere_; }

    void prepareCamera(const Camera& camera){setupCamera(camera);}
    void draw(const Simulation& sim,const Camera& camera,int selectedSquad=-1);
    void setCampaignBanner(const std::string& news,int funds){europe_.setCampaignBanner(news,funds);}
    void setPoliticalMap(bool on){politicalMap_=on;}
    void setSectorGrid(bool visible){sectorGrid_=visible;}
    // Optional CPU battlefield dressing: can be disabled for low-end benchmarking.
    void setBattlefieldDetail(bool enabled){battlefieldDetail_=enabled;}
    void setPhotoMode(bool enabled){photoMode_=enabled;}
    bool photoMode()const{return photoMode_;}
    bool battlefieldDetail() const{return battlefieldDetail_;}
    bool sectorGrid()const{return sectorGrid_;}
    bool politicalMap()const{return politicalMap_;}
    bool selectGlobalAt(int sx,int sy){return europe_.selectAt(sx,sy);}
    const std::optional<MapSelection>& globalSelection()const noexcept{return europe_.selection();}
    void setGlobalMapView(float zoom,float panX,float panY){globalZoom_=zoom;globalPanX_=panX;globalPanY_=panY;}
    void setGlobalReliefScale(float scale){europe_.setReliefScale(scale);}
    bool screenToGround(const Camera& camera,int sx,int sy,float& x,float& z)const;
    bool project(const Camera& camera,float x,float y,float z,int& sx,int& sy)const;
    bool savePPM(const std::string& file) const;
    int width() const {return w_;}
    int height() const {return h_;}
    const std::uint32_t* data() const {return color_.data();}
    std::uint32_t* mutablePixels() {return color_.data();}
    std::uint64_t lastTriangles() const {return triangleCount_;}
    void setTerrainDirectory(const std::string& directory) { terrain_.setTileDirectory(directory); }
    std::uint64_t terrainLoads() const {return terrain_.sectorLoads();}
    std::size_t terrainCacheBytes() const {return terrain_.cachedBytes();}
    static float terrainHeight(float x,float z);
private:
    struct Projected {float x,y,depth;bool valid;};
    void setupCamera(const Camera& camera);
    Projected projection(const Vec3& pos)const;
    enum class Surface : std::uint8_t { Solid, Water, Shadow, Terrain };
    void triangle(Vec3 a,Vec3 b,Vec3 c,std::uint32_t color,Surface surface=Surface::Solid,TextureMaterial material=TextureMaterial::None);
    void drawRiver(const Camera& camera);
    void drawRoads(const Camera& camera);
    void drawSky(const Camera& camera);
    void groundShadow(float x,float z,float height,float width,float lengthFactor);
    void drawFog();
    void drawPrecipitation();
    void drawEmissiveEffects();
    std::uint32_t shadeLocalLights(std::uint32_t color,float x,float y,float z,float normalY)const;
    void glowCircle(int sx,int sy,int radius,std::uint32_t color,float alpha);
    std::uint32_t waterColor(float x,float z) const;
    void applyEdgeAA();
    void worldLine(Vec3 a,Vec3 b,std::uint32_t rgb,int thickness=1);
    void line2d(float x0,float y0,float x1,float y1,std::uint32_t color,int t);
    void rect(int x,int y,int w,int h,std::uint32_t color);
    void box(Vec3 center,float sx,float sy,float sz,std::uint32_t color,TextureMaterial material=TextureMaterial::None);
    void drawTerrain(const Camera& camera);
    void drawProps(const Camera& camera);
    void drawBattleSmoke(const Camera& camera);
    void drawUnits(const Simulation& sim,const Camera& camera,int selectedSquad);
    void drawGlobal(const Simulation& sim);
    int w_=960,h_=600;
    std::vector<std::uint32_t> color_;
    std::vector<float> depth_;
    std::vector<std::uint32_t> aaScratch_;
    mutable TerrainWorld terrain_{16};
    EuropeBackdrop europe_;
    bool politicalMap_=true;
    bool sectorGrid_=false;
    bool battlefieldDetail_=true;
    bool photoMode_=false;
    TextureLibrary textures_;
    bool texturingEnabled_=true;
    float globalZoom_=1.f,globalPanX_=0.f,globalPanY_=0.f;
    VisualEffects effects_{};
    AtmosphereSettings atmosphere_{};
    float visualTime_=0.f;
    ShaderQuality shaderQuality_=ShaderQuality::Balanced;
    bool dynamicLights_=true,precipitation_=true;
    ExplosionEvent explosion_{};
    std::array<LocalLight,4> localLights_{};
    float localWetness_=0.f;
    Vec3 eye_,fwd_,right_,up_;
    float focal_=500;
    std::uint64_t triangleCount_=0;
};
}
