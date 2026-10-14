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
            mesh.indices.data(),count,mesh.staticVertexBytes,mesh.staticIndexCount,
            mesh.resourceId,mesh.staticRevision,mesh.dynamicRevision};
}
inline GpuSceneCamera makeGpuSceneCamera(const Camera& c,float centerHeight,AtmosphereSettings atmosphere,VisualEffects effects={}){
    GpuSceneCamera out{};
    out.firstPerson=c.firstPerson;out.eyeHeight=c.eyeHeight;out.fov=c.fov;
    out.focusX=c.focusX;out.focusY=centerHeight+(c.firstPerson?0.f:1.f);out.focusZ=c.focusZ;
    out.distance=c.distance;out.pitch=c.pitch;out.yaw=c.yaw;
    const auto light=atmosphereProfile(atmosphere);
    out.sunX=light.sunX;out.sunY=light.sunY;out.sunZ=light.sunZ;
    out.ambient=light.ambient;out.direct=light.direct;
    out.fogStart=light.fogNear;out.fogEnd=light.fogFar;
    out.sunR=light.sunR;out.sunG=light.sunG;out.sunB=light.sunB;out.shadowStrength=light.shadowOpacity;
    out.fogR=light.fogR;out.fogG=light.fogG;out.fogB=light.fogB;
    out.skyR=light.skyR;out.skyG=light.skyG;out.skyB=light.skyB;
    out.wind=effects.wind?.35f*light.windMultiplier:0.f;
    out.wetness=light.wetness;out.snowCover=light.snowCover;
    out.cloudCover=atmosphere.weather==Weather::Clear?.18f:.78f;
    out.waterReflections=effects.waterReflections;
    out.enabled=true;
    return out;
}
}
