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
    void clear(){vertices.clear();indices.clear();treeCount=waterTriangleCount=terrainTriangleCount=0;shadowFirstIndex=shadowIndexCount=shadowCasterCount=0;}
    std::size_t bytes() const {return vertices.size()*sizeof(SceneVertex)+indices.size()*sizeof(std::uint32_t);}
};
class SceneMeshBuilder {
public:
    void build(SceneMesh& mesh,TerrainWorld& terrain,const Simulation& sim,const Camera& camera,int selectedSquad,AtmosphereSettings atmosphere={});
    static void addTree(SceneMesh& mesh,float x,float y,float z,float scale=1.f,std::uint32_t seed=0);
};
}
