#pragma once
#include "gz/Renderer.hpp"
#include <cstdint>
#include <vector>
namespace gz {
// Data layout matches the D3D11 input layout (12 float32 components).
struct SceneVertex {
    float x,y,z,nx,ny,nz,u,v,material,windWeight,anchorX,anchorZ;
};
struct SceneMesh {
    std::vector<SceneVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::uint32_t treeCount=0;
    std::uint32_t waterTriangleCount=0;
    std::uint32_t terrainTriangleCount=0;
    std::uint32_t shadowFirstIndex=0,shadowIndexCount=0,shadowCasterCount=0;
    std::size_t staticVertexBytes=0,staticIndexCount=0;
    std::uint64_t resourceId=0,staticRevision=0,dynamicRevision=0;
    void clear(){vertices.clear();indices.clear();treeCount=waterTriangleCount=terrainTriangleCount=0;shadowFirstIndex=shadowIndexCount=shadowCasterCount=0;staticVertexBytes=staticIndexCount=0;resourceId=staticRevision=dynamicRevision=0;}
    std::size_t bytes() const {return vertices.size()*sizeof(SceneVertex)+indices.size()*sizeof(std::uint32_t);}
};
class SceneMeshBuilder {
public:
    SceneMeshBuilder();
    SceneMeshBuilder(const SceneMeshBuilder&)=delete;
    SceneMeshBuilder& operator=(const SceneMeshBuilder&)=delete;
    void build(SceneMesh& mesh,TerrainWorld& terrain,const Simulation& sim,const Camera& camera,int selectedSquad,AtmosphereSettings atmosphere={});
    static void addTree(SceneMesh& mesh,float x,float y,float z,float scale=1.f,std::uint32_t seed=0);
    void invalidate(){cacheValid_=false;}
    std::uint64_t staticBuildCount()const{return staticRevision_;}
    std::uint64_t dynamicUpdateCount()const{return dynamicRevision_;}
private:
    void buildStatic(SceneMesh& mesh,TerrainWorld& terrain,const Camera& camera,AtmosphereSettings atmosphere);
    SceneMesh staticCache_,units_;
    const TerrainWorld* cachedTerrain_=nullptr;
    std::uint64_t cachedTerrainRevision_=0,resourceId_=0,staticRevision_=0,dynamicRevision_=0;
    Camera cachedCamera_{};
    AtmosphereSettings cachedAtmosphere_{};
    bool cacheValid_=false;
};
}
