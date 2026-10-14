#include "VulkanScenePass.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <limits>
#include <vector>

namespace gz {
namespace {
std::vector<std::uint32_t> readSpirv(const char* path){
    std::ifstream f(path,std::ios::binary|std::ios::ate);
    if(!f)return {};
    const std::streamoff length=f.tellg();
    if(length<4 || length>8*1024*1024 || (length%4)!=0)return {};
    std::vector<std::uint32_t> words(std::size_t(length)/4u);
    f.seekg(0);f.read(reinterpret_cast<char*>(words.data()),length);
    if(!f||words.empty()||words[0]!=0x07230203u)return {};
    return words;
}
VkShaderModule makeShader(VkDevice device,const std::vector<std::uint32_t>& spirv){
    VkShaderModule module=VK_NULL_HANDLE;
    if(spirv.empty())return module;
    VkShaderModuleCreateInfo sc{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    sc.codeSize=spirv.size()*sizeof(std::uint32_t);sc.pCode=spirv.data();
    if(vkCreateShaderModule(device,&sc,nullptr,&module)!=VK_SUCCESS)return VK_NULL_HANDLE;
    return module;
}
}
bool VulkanScenePass::allocMemory(std::uint32_t bits,VkMemoryPropertyFlags required,std::uint32_t& out)const{
    VkPhysicalDeviceMemoryProperties props{};
    vkGetPhysicalDeviceMemoryProperties(physical_,&props);
    for(std::uint32_t i=0;i<props.memoryTypeCount;++i){
        if((bits&(1u<<i)) && (props.memoryTypes[i].propertyFlags&required)==required){out=i;return true;}
    }
    return false;
}
void VulkanScenePass::destroyBuffer(HostBuffer& b)noexcept{
    if(device_){
        if(b.mapped)vkUnmapMemory(device_,b.memory);
        if(b.buffer)vkDestroyBuffer(device_,b.buffer,nullptr);
        if(b.memory)vkFreeMemory(device_,b.memory,nullptr);
    }
    b={};
}
bool VulkanScenePass::makeBuffer(HostBuffer& target,VkDeviceSize size,VkBufferUsageFlags usage,std::string& error){
    if(target.buffer && target.capacity>=size)return true;
    destroyBuffer(target);
    if(size>budget_){error="Native GPU mesh buffer exceeds streaming budget";return false;}
    VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    ci.size=size;ci.usage=usage;ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    if(vkCreateBuffer(device_,&ci,nullptr,&target.buffer)!=VK_SUCCESS){error="vkCreateBuffer for mesh failed";return false;}
    VkMemoryRequirements req{};vkGetBufferMemoryRequirements(device_,target.buffer,&req);
    std::uint32_t type=0;
    if(!allocMemory(req.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,type)){
        if(!allocMemory(req.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,type)){
            error="No Vulkan host-visible mesh memory";destroyBuffer(target);return false;
        }
    }
    VkPhysicalDeviceMemoryProperties properties{};vkGetPhysicalDeviceMemoryProperties(physical_,&properties);
    target.coherent=(properties.memoryTypes[type].propertyFlags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)!=0;
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.allocationSize=req.size;ai.memoryTypeIndex=type;
    if(vkAllocateMemory(device_,&ai,nullptr,&target.memory)!=VK_SUCCESS ||
       vkBindBufferMemory(device_,target.buffer,target.memory,0)!=VK_SUCCESS ||
       vkMapMemory(device_,target.memory,0,VK_WHOLE_SIZE,0,&target.mapped)!=VK_SUCCESS){
        error="Could not allocate/map GPU mesh staging";destroyBuffer(target);return false;
    }
    target.capacity=size;
    return true;
}
bool VulkanScenePass::makeDepth(std::string& error){
    for(auto candidate:{VK_FORMAT_D32_SFLOAT,VK_FORMAT_D24_UNORM_S8_UINT,VK_FORMAT_D16_UNORM}){
        VkFormatProperties properties{};vkGetPhysicalDeviceFormatProperties(physical_,candidate,&properties);
        if(properties.optimalTilingFeatures&VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT){depthFormat_=candidate;break;}
    }
    if(depthFormat_==VK_FORMAT_UNDEFINED){error="No Vulkan depth/stencil format";return false;}
    VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    image.imageType=VK_IMAGE_TYPE_2D;image.format=depthFormat_;image.extent={extent_.width,extent_.height,1};
    image.mipLevels=1;image.arrayLayers=1;image.samples=VK_SAMPLE_COUNT_1_BIT;
    image.tiling=VK_IMAGE_TILING_OPTIMAL;image.usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    image.sharingMode=VK_SHARING_MODE_EXCLUSIVE;image.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    if(vkCreateImage(device_,&image,nullptr,&depth_)!=VK_SUCCESS){error="Could not create depth image";return false;}
    VkMemoryRequirements need{};vkGetImageMemoryRequirements(device_,depth_,&need);
    std::uint32_t type=0;
    if(!allocMemory(need.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,type) &&
       !allocMemory(need.memoryTypeBits,0,type)){error="No Vulkan depth image memory";return false;}
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.allocationSize=need.size;ai.memoryTypeIndex=type;
    if(vkAllocateMemory(device_,&ai,nullptr,&depthMemory_)!=VK_SUCCESS ||
       vkBindImageMemory(device_,depth_,depthMemory_,0)!=VK_SUCCESS){error="Depth memory allocation failed";return false;}
    VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view.image=depth_;view.viewType=VK_IMAGE_VIEW_TYPE_2D;view.format=depthFormat_;
    view.subresourceRange.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;
    view.subresourceRange.levelCount=1;view.subresourceRange.layerCount=1;
    if(vkCreateImageView(device_,&view,nullptr,&depthView_)!=VK_SUCCESS){error="Depth view creation failed";return false;}
    return true;
}
bool VulkanScenePass::makeRenderPass(std::string& error){
    std::array<VkAttachmentDescription,2> attachments{};
    attachments[0].format=colorFormat_;attachments[0].samples=VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;attachments[0].storeOp=VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;attachments[0].finalLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachments[1].format=depthFormat_;attachments[1].samples=VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;attachments[1].storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;attachments[1].finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkAttachmentReference color{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depth{1,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub{};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount=1;sub.pColorAttachments=&color;sub.pDepthStencilAttachment=&depth;
    std::array<VkSubpassDependency,2> deps{};
    deps[0].srcSubpass=VK_SUBPASS_EXTERNAL;deps[0].dstSubpass=0;
    deps[0].srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[0].dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    deps[0].dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    deps[1].srcSubpass=0;deps[1].dstSubpass=VK_SUBPASS_EXTERNAL;
    deps[1].srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[1].dstStageMask=VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    deps[1].srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    VkRenderPassCreateInfo create{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    create.attachmentCount=std::uint32_t(attachments.size());create.pAttachments=attachments.data();
    create.subpassCount=1;create.pSubpasses=&sub;create.dependencyCount=std::uint32_t(deps.size());create.pDependencies=deps.data();
    if(vkCreateRenderPass(device_,&create,nullptr,&pass_)!=VK_SUCCESS){error="vkCreateRenderPass failed";return false;}
    return true;
}
bool VulkanScenePass::makePipeline(std::string& error){
    const auto vertexSpirv=readSpirv("assets/shaders/vulkan/native_scene.vert.spv");
    const auto fragmentSpirv=readSpirv("assets/shaders/vulkan/native_scene.frag.spv");
    if(vertexSpirv.empty() || fragmentSpirv.empty()){
        error="GPU SPIR-V files missing: build the shader target with glslangValidator";return false;
    }
    VkShaderModule vs=makeShader(device_,vertexSpirv),fs=makeShader(device_,fragmentSpirv);
    if(!vs || !fs){if(vs)vkDestroyShaderModule(device_,vs,nullptr);if(fs)vkDestroyShaderModule(device_,fs,nullptr);
        error="Could not create GPU shader modules";return false;}
    const std::array<VkPipelineShaderStageCreateInfo,2> stages=[&]{
        std::array<VkPipelineShaderStageCreateInfo,2> s{};
        s[0].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        s[0].stage=VK_SHADER_STAGE_VERTEX_BIT;s[0].module=vs;s[0].pName="main";
        s[1].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        s[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;s[1].module=fs;s[1].pName="main";
        return s;
    }();
    VkVertexInputBindingDescription binding{0,sizeof(SceneVertex),VK_VERTEX_INPUT_RATE_VERTEX};
    std::array<VkVertexInputAttributeDescription,5> attributes{{
        {0,0,VK_FORMAT_R32G32B32_SFLOAT,std::uint32_t(offsetof(SceneVertex,x))},
        {1,0,VK_FORMAT_R32G32B32_SFLOAT,std::uint32_t(offsetof(SceneVertex,nx))},
        {2,0,VK_FORMAT_R32_SFLOAT,std::uint32_t(offsetof(SceneVertex,material))},
        {3,0,VK_FORMAT_R32_SFLOAT,std::uint32_t(offsetof(SceneVertex,windWeight))},
        {4,0,VK_FORMAT_R32G32_SFLOAT,std::uint32_t(offsetof(SceneVertex,anchorX))}
    }};
    VkPipelineVertexInputStateCreateInfo input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    input.vertexBindingDescriptionCount=1;input.pVertexBindingDescriptions=&binding;
    input.vertexAttributeDescriptionCount=std::uint32_t(attributes.size());input.pVertexAttributeDescriptions=attributes.data();
    VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkViewport viewport{0.f,0.f,float(extent_.width),float(extent_.height),0.f,1.f};
    VkRect2D scissor{{0,0},extent_};
    VkPipelineViewportStateCreateInfo viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount=1;viewportState.pViewports=&viewport;
    viewportState.scissorCount=1;viewportState.pScissors=&scissor;
    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    raster.polygonMode=VK_POLYGON_MODE_FILL;raster.cullMode=VK_CULL_MODE_NONE;
    raster.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;raster.lineWidth=1.f;
    VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisample.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo depth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depth.depthTestEnable=VK_TRUE;depth.depthWriteEnable=VK_TRUE;
    depth.depthCompareOp=VK_COMPARE_OP_LESS;
    VkPipelineColorBlendAttachmentState blend{};blend.colorWriteMask=VK_COLOR_COMPONENT_R_BIT|
        VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blendState{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    blendState.attachmentCount=1;blendState.pAttachments=&blend;
    VkPushConstantRange range{VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(GpuFrameConstants)};
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.pushConstantRangeCount=1;layoutInfo.pPushConstantRanges=&range;
    if(vkCreatePipelineLayout(device_,&layoutInfo,nullptr,&layout_)!=VK_SUCCESS){
        vkDestroyShaderModule(device_,vs,nullptr);vkDestroyShaderModule(device_,fs,nullptr);
        error="GPU pipeline layout failed";return false;
    }
    VkGraphicsPipelineCreateInfo graphics{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    graphics.stageCount=std::uint32_t(stages.size());graphics.pStages=stages.data();
    graphics.pVertexInputState=&input;graphics.pInputAssemblyState=&assembly;
    graphics.pViewportState=&viewportState;graphics.pRasterizationState=&raster;
    graphics.pMultisampleState=&multisample;graphics.pDepthStencilState=&depth;
    graphics.pColorBlendState=&blendState;graphics.layout=layout_;graphics.renderPass=pass_;
    const auto result=vkCreateGraphicsPipelines(device_,VK_NULL_HANDLE,1,&graphics,nullptr,&pipeline_);
    vkDestroyShaderModule(device_,vs,nullptr);vkDestroyShaderModule(device_,fs,nullptr);
    if(result!=VK_SUCCESS){error="Native Vulkan graphics pipeline creation failed";return false;}
    return true;
}
void VulkanScenePass::destroySwapchain()noexcept{
    if(device_){
        for(auto f:framebuffers_)vkDestroyFramebuffer(device_,f,nullptr);
        if(pipeline_)vkDestroyPipeline(device_,pipeline_,nullptr);
        if(layout_)vkDestroyPipelineLayout(device_,layout_,nullptr);
        if(pass_)vkDestroyRenderPass(device_,pass_,nullptr);
        if(depthView_)vkDestroyImageView(device_,depthView_,nullptr);
        if(depth_)vkDestroyImage(device_,depth_,nullptr);
        if(depthMemory_)vkFreeMemory(device_,depthMemory_,nullptr);
        for(auto view:colorViews_)vkDestroyImageView(device_,view,nullptr);
    }
    framebuffers_.clear();colorViews_.clear();pass_=VK_NULL_HANDLE;pipeline_=VK_NULL_HANDLE;
    layout_=VK_NULL_HANDLE;depth_=VK_NULL_HANDLE;depthMemory_=VK_NULL_HANDLE;
    depthView_=VK_NULL_HANDLE;depthFormat_=VK_FORMAT_UNDEFINED;ready_=false;
}
void VulkanScenePass::destroy()noexcept{
    destroySwapchain();destroyBuffer(vertex_);destroyBuffer(index_);
    device_=VK_NULL_HANDLE;physical_=VK_NULL_HANDLE;indices_=0;
}
bool VulkanScenePass::initialize(VkPhysicalDevice physical,VkDevice device,VkFormat format,VkExtent2D extent,
                                const std::vector<VkImage>& images,std::size_t budget,std::string& error){
    if(device_ && device_!=device)destroy();
    destroySwapchain();physical_=physical;device_=device;colorFormat_=format;extent_=extent;budget_=budget;
    if(images.empty()||!extent.width||!extent.height){error="GPU render targets missing";return false;}
    if(!makeDepth(error)||!makeRenderPass(error)||!makePipeline(error)){destroySwapchain();return false;}
    for(VkImage img:images){
        VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image=img;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.format=colorFormat_;
        vi.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
        vi.subresourceRange.levelCount=1;vi.subresourceRange.layerCount=1;
        VkImageView view=VK_NULL_HANDLE;
        if(vkCreateImageView(device_,&vi,nullptr,&view)!=VK_SUCCESS){error="Swapchain image view failed";destroySwapchain();return false;}
        colorViews_.push_back(view);
        const std::array<VkImageView,2> attachments{view,depthView_};
        VkFramebufferCreateInfo f{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        f.renderPass=pass_;f.attachmentCount=2;f.pAttachments=attachments.data();
        f.width=extent_.width;f.height=extent_.height;f.layers=1;
        VkFramebuffer framebuffer=VK_NULL_HANDLE;
        if(vkCreateFramebuffer(device_,&f,nullptr,&framebuffer)!=VK_SUCCESS){error="GPU framebuffer failed";destroySwapchain();return false;}
        framebuffers_.push_back(framebuffer);
    }
    ready_=true;return true;
}
bool VulkanScenePass::upload(const GeometryView& mesh,std::uint64_t& bytes,std::string& error){
    if(!ready_||!mesh.valid()||mesh.strideBytes!=sizeof(SceneVertex)){
        error="Native GPU mesh format invalid";return false;
    }
    const std::size_t ib=mesh.indexCount*sizeof(std::uint32_t);
    if(mesh.vertexBytes+ib>budget_){error="Native mesh exceeds configured 128 MiB budget";return false;}
    if(!makeBuffer(vertex_,std::max<std::size_t>(256u,mesh.vertexBytes),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,error)||
       !makeBuffer(index_,std::max<std::size_t>(256u,ib),VK_BUFFER_USAGE_INDEX_BUFFER_BIT,error))return false;
    std::memcpy(vertex_.mapped,mesh.vertices,mesh.vertexBytes);
    std::memcpy(index_.mapped,mesh.indices,ib);
    if(!vertex_.coherent){
        VkMappedMemoryRange flush{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
        flush.memory=vertex_.memory;flush.size=VK_WHOLE_SIZE;
        if(vkFlushMappedMemoryRanges(device_,1,&flush)!=VK_SUCCESS){error="Vertex flush failed";return false;}
    }
    if(!index_.coherent){
        VkMappedMemoryRange flush{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
        flush.memory=index_.memory;flush.size=VK_WHOLE_SIZE;
        if(vkFlushMappedMemoryRanges(device_,1,&flush)!=VK_SUCCESS){error="Index flush failed";return false;}
    }
    indices_=std::uint32_t(mesh.indexCount);
    bytes=mesh.vertexBytes+ib;
    return true;
}
void VulkanScenePass::record(VkCommandBuffer cmd,std::uint32_t image,const GpuFrameConstants& constants){
    if(!ready_||image>=framebuffers_.size())return;
    std::array<VkClearValue,2> clear{};
    clear[0].color={{.35f,.45f,.50f,1.f}};
    clear[1].depthStencil={1.f,0};
    VkRenderPassBeginInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rp.renderPass=pass_;rp.framebuffer=framebuffers_[image];rp.renderArea={{0,0},extent_};
    rp.clearValueCount=std::uint32_t(clear.size());rp.pClearValues=clear.data();
    vkCmdBeginRenderPass(cmd,&rp,VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_);
    const VkDeviceSize offset=0;
    vkCmdBindVertexBuffers(cmd,0,1,&vertex_.buffer,&offset);
    vkCmdBindIndexBuffer(cmd,index_.buffer,0,VK_INDEX_TYPE_UINT32);
    vkCmdPushConstants(cmd,layout_,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,
                       sizeof(GpuFrameConstants),&constants);
    vkCmdDrawIndexed(cmd,indices_,1,0,0,0);
    vkCmdEndRenderPass(cmd);
}
} // namespace gz
