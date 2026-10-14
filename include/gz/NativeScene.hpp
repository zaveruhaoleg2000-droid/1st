#pragma once
#include "gz/GpuSceneMath.hpp"
#include "gz/RenderBackend.hpp"
#include "gz/SceneMesh.hpp"
#include "gz/Atmosphere.hpp"
#include <cmath>
namespace gz {
inline GeometryView makeGeometryView(const SceneMesh& mesh) {
    // Shadows use a second CPU blended pass and must not render as opaque black
    // geometry in the first native GPU implementation.
    const std::size_t count=mesh.shadowFirstIndex ? mesh.shadowFirstIndex : mesh.indices.size();
    return {mesh.vertices.data(),mesh.vertices.size()*sizeof(SceneVertex),sizeof(SceneVertex),
            mesh.indices.data(),count};
}
inline GpuSceneCamera makeGpuSceneCamera(const Camera& c,float centerHeight,AtmosphereSettings atmosphere){
    GpuSceneCamera out{};
    out.focusX=c.focusX;out.focusY=centerHeight+1.f;out.focusZ=c.focusZ;
    out.distance=c.distance;out.pitch=c.pitch;out.yaw=c.yaw;
    const auto light=atmosphereProfile(atmosphere);
    out.sunX=light.sunX;out.sunY=light.sunY;out.sunZ=light.sunZ;
    out.ambient=light.ambient;out.direct=light.direct;
    out.fogStart=light.fogNear;out.fogEnd=light.fogFar;
    out.enabled=true;
    return out;
}
}
