#pragma once
#include "gz/RenderBackend.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>
namespace gz {
struct MeshUpdateRange {
    std::size_t offset=0,bytes=0;
};
struct MeshUploadPlan {
    MeshUpdateRange vertices,indices;
    std::size_t bytes()const{return vertices.bytes+indices.bytes;}
};
// Only advance this tracker after the corresponding copy has been recorded.
class MeshUploadTracker {
public:
    MeshUploadPlan plan(const GeometryView& mesh,bool newVertices=false,bool newIndices=false)const;
    void commit(const GeometryView& mesh){previous_=mesh;valid_=true;}
    void reset(){previous_={};valid_=false;}
private:
    GeometryView previous_{};
    bool valid_=false;
};
// Backend-agnostic immutable mesh staging. No device allocation occurs here:
// Vulkan can later turn a changed revision into persistent VkBuffers, while
// the DirectX compatibility slot can consume exactly the same data.
class GeometryStagingCache {
public:
    enum class Result {Invalid,OverBudget,Unchanged,Stored};
    explicit GeometryStagingCache(std::size_t budget=128u*1024u*1024u):budget_(budget){}
    Result stage(std::uint64_t meshId,const GeometryView& src);
    bool erase(std::uint64_t meshId) noexcept;
    void clear() noexcept;
    GeometryView view(std::uint64_t meshId)const noexcept;
    std::uint64_t revision(std::uint64_t meshId)const noexcept;
    std::size_t meshCount()const noexcept{return records_.size();}
    std::size_t residentBytes()const noexcept{return used_;}
    std::size_t budgetBytes()const noexcept{return budget_;}
    std::uint64_t changedUploads()const noexcept{return updates_;}
private:
    struct Entry {
        std::vector<std::uint8_t> vertices;
        std::vector<std::uint32_t> indices;
        std::size_t stride=0;
        std::uint64_t revision=0;
    };
    std::map<std::uint64_t,Entry> records_;
    std::size_t budget_=0,used_=0;
    std::uint64_t nextRevision_=1,updates_=0;
};
}
