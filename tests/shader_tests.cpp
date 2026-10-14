#include "gz/Shaders.hpp"
#include "gz/Renderer.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <stdexcept>
using namespace gz;
static void require(bool condition,const char* explanation){if(!condition)throw std::runtime_error(explanation);}
int main(){
    ShaderLibrary library;
    require(library.load("assets/shaders"),"349");
    require(library.scene().find("GZFoliage(")!=std::string::npos,"393");
    require(library.scene().find("GZTerrain(")!=std::string::npos,"460");
    require(library.scene().find("GZArchitecture(")!=std::string::npos,"527");
    require(library.scene().find("GZWater(")!=std::string::npos,"599");
    require(library.scene().find("GZ_LIGHTS")!=std::string::npos,"664");
    require(library.overlay().find("OverlayPS(")!=std::string::npos,"730");
    const std::string oldSource=library.scene();
    require(!library.load("assets/shaders/does_not_exist"),"848");
    require(library.scene()==oldSource && library.valid(),"908"); // transactionally preserved
    require(shaderBudget(ShaderQuality::Eco).localLights==0,"996");
    require(shaderBudget(ShaderQuality::Balanced).localLights==4,"1057");
    require(shaderBudget(ShaderQuality::Quality).localLights==4,"1123");
    require(!shaderBudget(ShaderQuality::Eco).waterHighlights,"1188");
    require(shaderBudget(ShaderQuality::Quality).fineTerrain,"1251");
    require(shaderQualityFromInt(999)==ShaderQuality::Quality,"1313");
    require(shaderQualityFromInt(-10)==ShaderQuality::Eco,"1376");
    require(shaderSurface(TextureMaterial::Leaves)==ShaderSurface::Foliage,"1435");
    require(shaderSurface(TextureMaterial::Water)==ShaderSurface::Water,"1511");
    require(shaderSurface(TextureMaterial::Brick)==ShaderSurface::Architecture,"1584");
    require(shaderSurface(TextureMaterial::GrassShort)==ShaderSurface::Terrain,"1664");
    Simulation sim(4);sim.newOperation(20);
    Renderer renderer;renderer.resize(360,240);
    renderer.setTerrainDirectory("assets/terrain");
    require(renderer.loadTextures("assets/textures/runtime",128)>=12,"1888");
    renderer.setAtmosphere({Weather::Rain,TimeOfDay::Sunset,true});
    renderer.setVisualTime(2.7f);
    Camera camera;camera.focusX=3090;camera.focusZ=1330;camera.distance=122;camera.pitch=.67f;
    std::array<std::uint64_t,3> checksums{};
    for(int q=0;q<3;++q){
        renderer.setShaderQuality(shaderQualityFromInt(q));
        require(int(renderer.shaderQuality())==q,"2290");
        renderer.draw(sim,camera,0);
        require(renderer.lastTriangles()>0,"2377");
        // FNV-1a on full frame: quality must actually affect CPU output.
        std::uint64_t checksum=1469598103934665603ull;
        for(int k=0;k<renderer.width()*renderer.height();++k){
            checksum^=renderer.data()[k];checksum*=1099511628211ull;
        }
        checksums[std::size_t(q)]=checksum;
        renderer.draw(sim,camera,0);
        std::uint64_t repeat=1469598103934665603ull;
        for(int k=0;k<renderer.width()*renderer.height();++k){
            repeat^=renderer.data()[k];repeat*=1099511628211ull;
        }
        require(checksum==repeat,"2964"); // deterministic with same simulation/time/quality
    }
    require(checksums[0]!=checksums[1],"3051");
    require(checksums[1]!=checksums[2],"3091");
    std::cout << "shader source validation + registry + quality render determinism: PASS\n";
}
