#pragma once
#include <vulkan/vulkan.h>
#include "gz/RenderBackend.hpp"
#include "gz/SceneMesh.hpp"
#include "gz/GeometryStaging.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace gz {
// Vulkan 1.0 native geometry pass. Independent from the CPU framebuffer upload.
// GPU buffer ownership and swapchain-dependent render pass live here; the game
// provides immutable SceneMesh views and camera constants through the RHI.
class VulkanScenePass {
    struct HostBuffer {
        VkBuffer buffer=VK_NULL_HANDLE;
        VkDeviceMemory memory=VK_NULL_HANDLE;
        void* mapped=nullptr;
        VkDeviceSize capacity=0;
        VkDeviceSize allocationBytes=0;
        bool coherent=false;
    };
    struct DeviceBuffer {
        VkBuffer buffer=VK_NULL_HANDLE;
        VkDeviceMemory memory=VK_NULL_HANDLE;
        VkDeviceSize capacity=0,allocationBytes=0;
    };
    struct ImageResource {
        VkImage image=VK_NULL_HANDLE;
        VkDeviceMemory memory=VK_NULL_HANDLE;
        VkImageView view=VK_NULL_HANDLE;
        VkDeviceSize allocationBytes=0;
    };
    VkPhysicalDevice physical_=VK_NULL_HANDLE;
    VkDevice device_=VK_NULL_HANDLE;
    VkExtent2D extent_{};
    VkFormat colorFormat_=VK_FORMAT_UNDEFINED;
    VkFormat depthFormat_=VK_FORMAT_UNDEFINED;
    VkRenderPass pass_=VK_NULL_HANDLE;
    VkPipelineLayout layout_=VK_NULL_HANDLE;
    VkPipeline pipeline_=VK_NULL_HANDLE;
    VkPipeline skyPipeline_=VK_NULL_HANDLE,shadowPipeline_=VK_NULL_HANDLE;
    VkRenderPass shadowPass_=VK_NULL_HANDLE;
    VkFramebuffer shadowFramebuffer_=VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorLayout_=VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_=VK_NULL_HANDLE;
    VkDescriptorSet descriptor_=VK_NULL_HANDLE;
    VkSampler materialSampler_=VK_NULL_HANDLE,shadowSampler_=VK_NULL_HANDLE;
    ImageResource materials_,shadow_,msaaColor_;
    HostBuffer uniforms_,textureStaging_;
    std::vector<VkBufferImageCopy> textureCopies_;
    bool texturesPending_=false;
    VkSampleCountFlagBits samples_=VK_SAMPLE_COUNT_1_BIT;
    std::uint32_t shadowSize_=2048,materialCount_=0;
    VkDeviceSize depthAllocationBytes_=0;
    VkImage depth_=VK_NULL_HANDLE;
    VkDeviceMemory depthMemory_=VK_NULL_HANDLE;
    VkImageView depthView_=VK_NULL_HANDLE;
    std::vector<VkImageView> colorViews_;
    std::vector<VkFramebuffer> framebuffers_;
    HostBuffer vertex_,index_;
    DeviceBuffer vertexDevice_,indexDevice_;
    MeshUploadTracker uploads_;
    MeshUploadPlan pending_{};
    GeometryView pendingMesh_{};
    std::uint32_t indices_=0;
    std::size_t budget_=128u*1024u*1024u;
    bool ready_=false;
    bool allocMemory(std::uint32_t allowedBits,VkMemoryPropertyFlags required,
                     std::uint32_t& result)const;
    bool makeBuffer(HostBuffer& target,VkDeviceSize size,VkBufferUsageFlags usage,std::string& error);
    void destroyBuffer(HostBuffer& target)noexcept;
    bool makeDeviceBuffer(DeviceBuffer& target,VkDeviceSize size,VkBufferUsageFlags usage,std::string& error);
    void destroyDeviceBuffer(DeviceBuffer& target)noexcept;
    bool makeDepth(std::string& error);
    bool makeRenderPass(std::string& error);
    bool makePipeline(std::string& error);
    bool makeImage(ImageResource& target,VkFormat format,std::uint32_t width,std::uint32_t height,
                   std::uint32_t levels,std::uint32_t layers,VkSampleCountFlagBits samples,
                   VkImageUsageFlags usage,VkImageAspectFlags aspect,std::string& error);
    void destroyImage(ImageResource& image)noexcept;
    bool makeMaterials(std::string& error);
    bool makeShadow(std::string& error);
    bool makeDescriptors(std::string& error);
    void recordTextures(VkCommandBuffer command);
    void destroySwapchain()noexcept;
public:
    ~VulkanScenePass(){destroy();}
    VulkanScenePass()=default;
    VulkanScenePass(const VulkanScenePass&)=delete;
    VulkanScenePass& operator=(const VulkanScenePass&)=delete;
    bool initialize(VkPhysicalDevice physical,VkDevice device,VkFormat format,VkExtent2D extent,
                    const std::vector<VkImage>& images,std::size_t budget,std::string& error);
    void destroy()noexcept;
    bool ready()const noexcept{return ready_;}
    bool upload(const GeometryView& mesh,const GpuFrameConstants& constants,std::uint64_t& bytes,std::string& error);
    void record(VkCommandBuffer command,std::uint32_t swapImage,const GpuFrameConstants& constants);
    std::uint32_t triangleCount()const noexcept{return indices_/3;}
    std::uint64_t residentBytes()const noexcept{return vertex_.allocationBytes+index_.allocationBytes+vertexDevice_.allocationBytes+indexDevice_.allocationBytes;}
    std::uint64_t graphicsResidentBytes()const noexcept{return residentBytes()+materials_.allocationBytes+shadow_.allocationBytes+msaaColor_.allocationBytes+depthAllocationBytes_+uniforms_.allocationBytes+textureStaging_.allocationBytes;}
    std::uint32_t shadowMapSize()const noexcept{return shadowSize_;}
    std::uint32_t sampleCount()const noexcept{return std::uint32_t(samples_);}
    std::uint32_t materialCount()const noexcept{return materialCount_;}
};
} // namespace gz
