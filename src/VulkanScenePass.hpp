#pragma once
#include <vulkan/vulkan.h>
#include "gz/RenderBackend.hpp"
#include "gz/SceneMesh.hpp"
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
        bool coherent=false;
    };
    VkPhysicalDevice physical_=VK_NULL_HANDLE;
    VkDevice device_=VK_NULL_HANDLE;
    VkExtent2D extent_{};
    VkFormat colorFormat_=VK_FORMAT_UNDEFINED;
    VkFormat depthFormat_=VK_FORMAT_UNDEFINED;
    VkRenderPass pass_=VK_NULL_HANDLE;
    VkPipelineLayout layout_=VK_NULL_HANDLE;
    VkPipeline pipeline_=VK_NULL_HANDLE;
    VkImage depth_=VK_NULL_HANDLE;
    VkDeviceMemory depthMemory_=VK_NULL_HANDLE;
    VkImageView depthView_=VK_NULL_HANDLE;
    std::vector<VkImageView> colorViews_;
    std::vector<VkFramebuffer> framebuffers_;
    HostBuffer vertex_,index_;
    std::uint32_t indices_=0;
    std::size_t budget_=128u*1024u*1024u;
    bool ready_=false;
    bool allocMemory(std::uint32_t allowedBits,VkMemoryPropertyFlags required,
                     std::uint32_t& result)const;
    bool makeBuffer(HostBuffer& target,VkDeviceSize size,VkBufferUsageFlags usage,std::string& error);
    void destroyBuffer(HostBuffer& target)noexcept;
    bool makeDepth(std::string& error);
    bool makeRenderPass(std::string& error);
    bool makePipeline(std::string& error);
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
    bool upload(const GeometryView& mesh,std::uint64_t& bytes,std::string& error);
    void record(VkCommandBuffer command,std::uint32_t swapImage,const GpuFrameConstants& constants);
    std::uint32_t triangleCount()const noexcept{return indices_/3;}
};
} // namespace gz
