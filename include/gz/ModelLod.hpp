#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
namespace gz {
enum class ModelLod : std::uint8_t { Near=0,Medium=1,Far=2,Silhouette=3 };
inline ModelLod modelLod(float meters,float scale=1.f){
    if(!std::isfinite(meters)||!std::isfinite(scale)||scale<=0)return ModelLod::Silhouette;
    const float d=std::max(0.f,meters)/scale;
    return d<40?ModelLod::Near:d<120?ModelLod::Medium:d<320?ModelLod::Far:ModelLod::Silhouette;
}
inline ModelLod stableModelLod(float meters,ModelLod previous,float scale=1.f){
    constexpr std::array<float,3> edges{40,120,320};int level=int(previous);float d=std::max(0.f,meters)/std::max(.01f,scale);
    while(level<3&&d>edges[level]*1.12f)++level;
    while(level>0&&d<edges[level-1]*.88f)--level;
    return ModelLod(level);
}
}
