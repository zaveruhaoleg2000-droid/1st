#include "gz/Shaders.hpp"
#include <fstream>
#include <iterator>
namespace gz {
namespace {
// Avoid accidentally passing an empty or obviously corrupt file to D3DCompile.
bool readText(const std::filesystem::path& p,std::string& result){
    std::ifstream file(p,std::ios::binary|std::ios::ate);
    if(!file || file.tellg()<32 || file.tellg()>1024*1024)return false;
    file.seekg(0);
    result.assign(std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>());
    return !result.empty() && result.find('\0')==std::string::npos;
}
}
bool ShaderLibrary::load(const std::filesystem::path& root){
    std::string newScene,newOverlay;
    if(!readText(root/"scene.hlsl",newScene) || !readText(root/"overlay.hlsl",newOverlay)) {
        error_="Shader files missing, invalid or larger than 1 MiB in "+root.string();return false;
    }
    if(newScene.find("VSMain(")==std::string::npos || newScene.find("PSMain(")==std::string::npos ||
       newScene.find("cbuffer Frame")==std::string::npos ||
       newOverlay.find("OverlayVS(")==std::string::npos || newOverlay.find("OverlayPS(")==std::string::npos){
        error_="Missing required HLSL entry point or Frame constant buffer";return false;
    }
    scene_.swap(newScene);overlay_.swap(newOverlay);error_.clear();return true;
}
}
