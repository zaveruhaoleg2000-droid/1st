#include "gz/GeometryStaging.hpp"
#include <cstring>
#include <limits>
namespace gz {
MeshUploadPlan MeshUploadTracker::plan(const GeometryView& mesh,bool newVertices,bool newIndices)const {
    const bool full=!valid_ || !mesh.resourceId || !mesh.staticRevision || !mesh.dynamicRevision ||
        mesh.resourceId!=previous_.resourceId || mesh.staticRevision!=previous_.staticRevision ||
        mesh.staticVertexBytes!=previous_.staticVertexBytes || mesh.staticIndexCount!=previous_.staticIndexCount ||
        mesh.strideBytes!=previous_.strideBytes;
    const bool dynamic=full || mesh.dynamicRevision!=previous_.dynamicRevision ||
        mesh.vertexBytes!=previous_.vertexBytes || mesh.indexCount!=previous_.indexCount;
    MeshUploadPlan result;
    if(full || newVertices)result.vertices={0,mesh.vertexBytes};
    else if(dynamic)result.vertices={mesh.staticVertexBytes,mesh.vertexBytes-mesh.staticVertexBytes};
    if(full || newIndices)result.indices={0,mesh.indexCount*sizeof(std::uint32_t)};
    else if(dynamic)result.indices={mesh.staticIndexCount*sizeof(std::uint32_t),(mesh.indexCount-mesh.staticIndexCount)*sizeof(std::uint32_t)};
    return result;
}
GeometryStagingCache::Result GeometryStagingCache::stage(std::uint64_t id,const GeometryView& src){
    // Keep hostile/corrupted geometry from indexing outside GPU vertex buffers.
    if(!src.valid()||src.vertexBytes>128u*1024u*1024u||
       src.strideBytes>4096||src.strideBytes<sizeof(float)*3||
       src.indexCount>std::numeric_limits<std::size_t>::max()/sizeof(std::uint32_t))return Result::Invalid;
    const std::size_t nVertices=src.vertexBytes/src.strideBytes;
    if(nVertices==0||nVertices>std::numeric_limits<std::uint32_t>::max())return Result::Invalid;
    for(std::size_t i=0;i<src.indexCount;++i)if(src.indices[i]>=nVertices)return Result::Invalid;
    const std::size_t indexBytes=src.indexCount*sizeof(std::uint32_t);
    if(src.vertexBytes>std::numeric_limits<std::size_t>::max()-indexBytes)return Result::Invalid;
    const std::size_t incoming=src.vertexBytes+indexBytes;
    auto existing=records_.find(id);
    const std::size_t prior=existing!=records_.end()?
        existing->second.vertices.size()+existing->second.indices.size()*sizeof(std::uint32_t):0;
    if(incoming>budget_||used_-prior>budget_-incoming)return Result::OverBudget;
    if(existing!=records_.end()){
        const auto& old=existing->second;
        if(old.stride==src.strideBytes&&old.vertices.size()==src.vertexBytes&&
           old.indices.size()==src.indexCount&&
           std::memcmp(old.vertices.data(),src.vertices,src.vertexBytes)==0&&
           std::memcmp(old.indices.data(),src.indices,indexBytes)==0)return Result::Unchanged;
    }
    Entry fresh;
    fresh.vertices.resize(src.vertexBytes);
    std::memcpy(fresh.vertices.data(),src.vertices,src.vertexBytes);
    fresh.indices.assign(src.indices,src.indices+src.indexCount);
    fresh.stride=src.strideBytes;
    fresh.revision=nextRevision_++;
    if(existing!=records_.end())existing->second=std::move(fresh);
    else records_.emplace(id,std::move(fresh));
    used_=used_-prior+incoming;
    ++updates_;
    return Result::Stored;
}
bool GeometryStagingCache::erase(std::uint64_t id)noexcept{
    const auto it=records_.find(id);
    if(it==records_.end())return false;
    used_-=it->second.vertices.size()+it->second.indices.size()*sizeof(std::uint32_t);
    records_.erase(it);return true;
}
void GeometryStagingCache::clear()noexcept{records_.clear();used_=0;}
GeometryView GeometryStagingCache::view(std::uint64_t id)const noexcept{
    const auto it=records_.find(id);
    if(it==records_.end())return {};
    const auto& m=it->second;
    return {m.vertices.data(),m.vertices.size(),m.stride,m.indices.data(),m.indices.size()};
}
std::uint64_t GeometryStagingCache::revision(std::uint64_t id)const noexcept{
    const auto it=records_.find(id);return it==records_.end()?0:it->second.revision;
}
}
