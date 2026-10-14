#include "VulkanScenePass.hpp"
#include "gz/TacticalHud.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <cstdlib>
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
    target.allocationBytes=req.size;
    return true;
}
void VulkanScenePass::destroyDeviceBuffer(DeviceBuffer& target)noexcept{
    if(device_){
        if(target.buffer)vkDestroyBuffer(device_,target.buffer,nullptr);
        if(target.memory)vkFreeMemory(device_,target.memory,nullptr);
    }
    target={};
}
bool VulkanScenePass::makeDeviceBuffer(DeviceBuffer& target,VkDeviceSize size,VkBufferUsageFlags usage,std::string& error){
    if(target.buffer && target.capacity>=size)return true;
    destroyDeviceBuffer(target);
    VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    ci.size=size;ci.usage=usage|VK_BUFFER_USAGE_TRANSFER_DST_BIT;ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    if(vkCreateBuffer(device_,&ci,nullptr,&target.buffer)!=VK_SUCCESS){error="Could not create device-local mesh buffer";return false;}
    VkMemoryRequirements need{};vkGetBufferMemoryRequirements(device_,target.buffer,&need);
    std::uint32_t type=0;
    if(!allocMemory(need.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,type)){
        error="No device-local mesh memory";destroyDeviceBuffer(target);return false;
    }
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.allocationSize=need.size;ai.memoryTypeIndex=type;
    if(vkAllocateMemory(device_,&ai,nullptr,&target.memory)!=VK_SUCCESS ||
       vkBindBufferMemory(device_,target.buffer,target.memory,0)!=VK_SUCCESS){
        error="Could not allocate device-local mesh memory";destroyDeviceBuffer(target);return false;
    }
    target.capacity=size;target.allocationBytes=need.size;
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
    image.mipLevels=1;image.arrayLayers=1;image.samples=samples_;
    image.tiling=VK_IMAGE_TILING_OPTIMAL;image.usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    image.sharingMode=VK_SHARING_MODE_EXCLUSIVE;image.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    if(vkCreateImage(device_,&image,nullptr,&depth_)!=VK_SUCCESS){error="Could not create depth image";return false;}
    VkMemoryRequirements need{};vkGetImageMemoryRequirements(device_,depth_,&need);
    std::uint32_t type=0;
    if(!allocMemory(need.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,type) &&
       !allocMemory(need.memoryTypeBits,0,type)){error="No Vulkan depth image memory";return false;}
    depthAllocationBytes_=need.size;
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
    const bool aa=samples_!=VK_SAMPLE_COUNT_1_BIT;
    std::array<VkAttachmentDescription,3> attachments{};
    attachments[0].format=colorFormat_;attachments[0].samples=samples_;
    attachments[0].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp=aa?VK_ATTACHMENT_STORE_OP_DONT_CARE:VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;attachments[0].stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout=aa?VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachments[1].format=depthFormat_;attachments[1].samples=samples_;
    attachments[1].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;attachments[1].storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;attachments[1].stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;attachments[1].finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[2]=attachments[0];attachments[2].samples=VK_SAMPLE_COUNT_1_BIT;
    attachments[2].loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;attachments[2].storeOp=VK_ATTACHMENT_STORE_OP_STORE;
    attachments[2].finalLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference color{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},depth{1,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkAttachmentReference resolve{2,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub{};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount=1;sub.pColorAttachments=&color;sub.pDepthStencilAttachment=&depth;
    if(aa)sub.pResolveAttachments=&resolve;
    std::array<VkSubpassDependency,2> deps{};
    deps[0].srcSubpass=VK_SUBPASS_EXTERNAL;deps[0].dstSubpass=0;
    deps[0].srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    deps[0].srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    deps[0].dstStageMask=deps[0].srcStageMask;
    deps[0].dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    deps[1].srcSubpass=0;deps[1].dstSubpass=VK_SUBPASS_EXTERNAL;
    deps[1].srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;deps[1].dstStageMask=VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    deps[1].srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    VkRenderPassCreateInfo create{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    create.attachmentCount=aa?3:2;create.pAttachments=attachments.data();create.subpassCount=1;create.pSubpasses=&sub;
    create.dependencyCount=2;create.pDependencies=deps.data();
    if(vkCreateRenderPass(device_,&create,nullptr,&pass_)!=VK_SUCCESS){error="Native render pass creation failed";return false;}
    return true;
}
bool VulkanScenePass::makePipeline(std::string& error){
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.setLayoutCount=1;layoutInfo.pSetLayouts=&descriptorLayout_;
    if(vkCreatePipelineLayout(device_,&layoutInfo,nullptr,&layout_)!=VK_SUCCESS){error="Native pipeline layout creation failed";return false;}
    auto load=[](const char* name){
        auto spirv=readSpirv((std::string("assets/shaders/vulkan/")+name+".spv").c_str());
#ifdef GZ_SHADER_DIR
        if(spirv.empty())spirv=readSpirv((std::string(GZ_SHADER_DIR)+"/"+name+".spv").c_str());
#endif
        return spirv;
    };
    auto make=[&](const char* prefix,VkRenderPass pass,VkPipeline& out,bool shadow,bool sky){
        const bool ui=std::strcmp(prefix,"native_ui")==0;
        const auto vs=makeShader(device_,load((std::string(prefix)+".vert").c_str()));
        const auto fs=makeShader(device_,load((std::string(prefix)+".frag").c_str()));
        if(!vs||!fs){if(vs)vkDestroyShaderModule(device_,vs,nullptr);if(fs)vkDestroyShaderModule(device_,fs,nullptr);
            error=std::string("Missing/invalid native shader: ")+prefix;return false;}
        std::array<VkPipelineShaderStageCreateInfo,2> stages{};
        stages[0].sType=stages[1].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[0].module=vs;stages[0].pName="main";
        stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;stages[1].module=fs;stages[1].pName="main";
        VkVertexInputBindingDescription binding{0,std::uint32_t(ui?sizeof(HudVertex):sizeof(SceneVertex)),VK_VERTEX_INPUT_RATE_VERTEX};
        const std::array<VkVertexInputAttributeDescription,6> attributes{{
            {0,0,VK_FORMAT_R32G32B32_SFLOAT,std::uint32_t(offsetof(SceneVertex,x))},
            {1,0,VK_FORMAT_R32G32B32_SFLOAT,std::uint32_t(offsetof(SceneVertex,nx))},
            {2,0,VK_FORMAT_R32_SFLOAT,std::uint32_t(offsetof(SceneVertex,material))},
            {3,0,VK_FORMAT_R32_SFLOAT,std::uint32_t(offsetof(SceneVertex,windWeight))},
            {4,0,VK_FORMAT_R32G32_SFLOAT,std::uint32_t(offsetof(SceneVertex,anchorX))},
            {5,0,VK_FORMAT_R32G32_SFLOAT,std::uint32_t(offsetof(SceneVertex,u))}
        }};
        const std::array<VkVertexInputAttributeDescription,3> uiAttributes{{
            {0,0,VK_FORMAT_R32G32_SFLOAT,0},{1,0,VK_FORMAT_R32G32_SFLOAT,8},{2,0,VK_FORMAT_R32G32B32A32_SFLOAT,16}}};
        VkPipelineVertexInputStateCreateInfo input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        if(!sky){input.vertexBindingDescriptionCount=1;input.pVertexBindingDescriptions=&binding;
            input.vertexAttributeDescriptionCount=ui?3:std::uint32_t(attributes.size());input.pVertexAttributeDescriptions=ui?uiAttributes.data():attributes.data();}
        VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        const VkExtent2D size=shadow?VkExtent2D{shadowSize_,shadowSize_}:extent_;
        VkViewport viewport{0,0,float(size.width),float(size.height),0,1};VkRect2D scissor{{0,0},size};
        VkPipelineViewportStateCreateInfo viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewportState.viewportCount=viewportState.scissorCount=1;viewportState.pViewports=&viewport;viewportState.pScissors=&scissor;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        raster.polygonMode=VK_POLYGON_MODE_FILL;raster.cullMode=VK_CULL_MODE_NONE;raster.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;raster.lineWidth=1;
        raster.depthBiasEnable=shadow;raster.depthBiasConstantFactor=shadow?1.5f:0.f;raster.depthBiasSlopeFactor=shadow?1.7f:0.f;
        VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisample.rasterizationSamples=shadow?VK_SAMPLE_COUNT_1_BIT:samples_;
        VkPipelineDepthStencilStateCreateInfo depth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        depth.depthTestEnable=depth.depthWriteEnable=(sky||ui)?VK_FALSE:VK_TRUE;depth.depthCompareOp=VK_COMPARE_OP_LESS_OR_EQUAL;
        VkPipelineColorBlendAttachmentState blend{};blend.colorWriteMask=15;
        if(ui){blend.blendEnable=VK_TRUE;blend.srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;blend.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blend.colorBlendOp=VK_BLEND_OP_ADD;blend.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE;blend.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;blend.alphaBlendOp=VK_BLEND_OP_ADD;}
        VkPipelineColorBlendStateCreateInfo blendState{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        blendState.attachmentCount=shadow?0:1;blendState.pAttachments=shadow?nullptr:&blend;
        VkGraphicsPipelineCreateInfo graphics{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        graphics.stageCount=2;graphics.pStages=stages.data();graphics.pVertexInputState=&input;graphics.pInputAssemblyState=&assembly;
        graphics.pViewportState=&viewportState;graphics.pRasterizationState=&raster;graphics.pMultisampleState=&multisample;
        graphics.pDepthStencilState=&depth;graphics.pColorBlendState=&blendState;graphics.layout=layout_;graphics.renderPass=pass;
        const auto result=vkCreateGraphicsPipelines(device_,VK_NULL_HANDLE,1,&graphics,nullptr,&out);
        vkDestroyShaderModule(device_,vs,nullptr);vkDestroyShaderModule(device_,fs,nullptr);
        if(result!=VK_SUCCESS){error=std::string("Native graphics pipeline failed: ")+prefix;return false;}return true;
    };
    return make("native_scene",pass_,pipeline_,false,false) && make("native_sky",pass_,skyPipeline_,false,true) &&
        make("native_shadow",shadowPass_,shadowPipeline_,true,false) && make("native_ui",pass_,uiPipeline_,false,false);
}
void VulkanScenePass::destroySwapchain()noexcept{
    if(device_){
        for(auto f:framebuffers_)vkDestroyFramebuffer(device_,f,nullptr);
        if(pipeline_)vkDestroyPipeline(device_,pipeline_,nullptr);
        if(uiPipeline_)vkDestroyPipeline(device_,uiPipeline_,nullptr);
        if(skyPipeline_)vkDestroyPipeline(device_,skyPipeline_,nullptr);
        if(shadowPipeline_)vkDestroyPipeline(device_,shadowPipeline_,nullptr);
        if(shadowFramebuffer_)vkDestroyFramebuffer(device_,shadowFramebuffer_,nullptr);
        if(shadowPass_)vkDestroyRenderPass(device_,shadowPass_,nullptr);
        if(descriptorPool_)vkDestroyDescriptorPool(device_,descriptorPool_,nullptr);
        if(descriptorLayout_)vkDestroyDescriptorSetLayout(device_,descriptorLayout_,nullptr);
        if(materialSampler_)vkDestroySampler(device_,materialSampler_,nullptr);
        if(shadowSampler_)vkDestroySampler(device_,shadowSampler_,nullptr);
        if(layout_)vkDestroyPipelineLayout(device_,layout_,nullptr);
        if(pass_)vkDestroyRenderPass(device_,pass_,nullptr);
        if(depthView_)vkDestroyImageView(device_,depthView_,nullptr);
        if(depth_)vkDestroyImage(device_,depth_,nullptr);
        if(depthMemory_)vkFreeMemory(device_,depthMemory_,nullptr);
        for(auto view:colorViews_)vkDestroyImageView(device_,view,nullptr);
    }
    destroyImage(macro_);destroyBuffer(macroStaging_);macroCopies_.clear();macroPending_=false;macroRevision_=0;macroResolution_=0;
    if(glyphSampler_)vkDestroySampler(device_,glyphSampler_,nullptr);glyphSampler_=VK_NULL_HANDLE;
    destroyImage(glyphs_);destroyBuffer(glyphStaging_);destroyBuffer(uiVertices_);destroyBuffer(uiIndices_);uiIndexCount_=0;uiPipeline_=VK_NULL_HANDLE;
    destroyImage(normals_);destroyBuffer(normalStaging_);normalCopies_.clear();destroyImage(materials_);destroyImage(shadow_);destroyImage(msaaColor_);
    destroyBuffer(uniforms_);destroyBuffer(textureStaging_);textureCopies_.clear();texturesPending_=false;
    skyPipeline_=shadowPipeline_=VK_NULL_HANDLE;shadowPass_=VK_NULL_HANDLE;shadowFramebuffer_=VK_NULL_HANDLE;
    descriptorLayout_=VK_NULL_HANDLE;descriptorPool_=VK_NULL_HANDLE;descriptor_=VK_NULL_HANDLE;
    materialSampler_=shadowSampler_=VK_NULL_HANDLE;depthAllocationBytes_=0;materialCount_=0;
    framebuffers_.clear();colorViews_.clear();pass_=VK_NULL_HANDLE;pipeline_=VK_NULL_HANDLE;
    layout_=VK_NULL_HANDLE;depth_=VK_NULL_HANDLE;depthMemory_=VK_NULL_HANDLE;
    depthView_=VK_NULL_HANDLE;depthFormat_=VK_FORMAT_UNDEFINED;ready_=false;
}
void VulkanScenePass::destroy()noexcept{
    destroySwapchain();destroyBuffer(vertex_);destroyBuffer(index_);
    destroyDeviceBuffer(vertexDevice_);destroyDeviceBuffer(indexDevice_);
    uploads_.reset();pending_={};pendingMesh_={};
    device_=VK_NULL_HANDLE;physical_=VK_NULL_HANDLE;indices_=0;
}
bool VulkanScenePass::initialize(VkPhysicalDevice physical,VkDevice device,VkFormat format,VkExtent2D extent,
                                const std::vector<VkImage>& images,std::size_t budget,std::string& error){
    if(device_ && device_!=device)destroy();
    destroySwapchain();physical_=physical;device_=device;colorFormat_=format;extent_=extent;budget_=budget;
    if(images.empty()||!extent.width||!extent.height){error="GPU render targets missing";return false;}
    VkPhysicalDeviceProperties properties{};vkGetPhysicalDeviceProperties(physical_,&properties);
    const auto supported=properties.limits.framebufferColorSampleCounts&properties.limits.framebufferDepthSampleCounts;
    const char* quality=std::getenv("GZ_GPU_QUALITY");
    const bool low=quality&&std::strcmp(quality,"low")==0;
    const bool medium=quality&&std::strcmp(quality,"medium")==0;
    samples_=!low&&(supported&VK_SAMPLE_COUNT_4_BIT)?VK_SAMPLE_COUNT_4_BIT:VK_SAMPLE_COUNT_1_BIT;
    shadowSize_=low?512u:(medium?1024u:2048u);
    if(!makeDepth(error)||!makeRenderPass(error)||!makeMaterials(error)||!makeShadow(error)||!makeInterface(error)||!prepareMacro({},error)||!makeDescriptors(error) ||
       (samples_!=VK_SAMPLE_COUNT_1_BIT&&!makeImage(msaaColor_,colorFormat_,extent.width,extent.height,1,1,samples_,
          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,VK_IMAGE_ASPECT_COLOR_BIT,error))||!makePipeline(error)){
        destroySwapchain();return false;
    }
    for(VkImage img:images){
        VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image=img;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.format=colorFormat_;
        vi.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
        vi.subresourceRange.levelCount=1;vi.subresourceRange.layerCount=1;
        VkImageView view=VK_NULL_HANDLE;
        if(vkCreateImageView(device_,&vi,nullptr,&view)!=VK_SUCCESS){error="Swapchain image view failed";destroySwapchain();return false;}
        colorViews_.push_back(view);
        const bool aa=samples_!=VK_SAMPLE_COUNT_1_BIT;
        const std::array<VkImageView,3> attachments{aa?msaaColor_.view:view,depthView_,view};
        VkFramebufferCreateInfo f{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        f.renderPass=pass_;f.attachmentCount=aa?3:2;f.pAttachments=attachments.data();
        f.width=extent_.width;f.height=extent_.height;f.layers=1;
        VkFramebuffer framebuffer=VK_NULL_HANDLE;
        if(vkCreateFramebuffer(device_,&f,nullptr,&framebuffer)!=VK_SUCCESS){error="GPU framebuffer failed";destroySwapchain();return false;}
        framebuffers_.push_back(framebuffer);
    }
    ready_=true;return true;
}
bool VulkanScenePass::upload(const RenderSubmission& submission,const GpuFrameConstants& constants,std::uint64_t& bytes,std::string& error){
    const auto& mesh=submission.worldMesh;
    bytes=0;pending_={};
    if(!ready_||!mesh.valid()||mesh.strideBytes!=sizeof(SceneVertex)){
        error="Native GPU mesh format invalid";return false;
    }
    // The preceding submit fence has completed before upload() is called.
    if(!texturesPending_ && normalStaging_.buffer)destroyBuffer(normalStaging_);
    if(!texturesPending_ && textureStaging_.buffer)destroyBuffer(textureStaging_);
    if(!texturesPending_ && glyphStaging_.buffer)destroyBuffer(glyphStaging_);
    if(!macroPending_&&macroStaging_.buffer)destroyBuffer(macroStaging_);
    if(!prepareMacro(submission.terrainMacro,error))return false;
    uiIndexCount_=0;uiBytes_=0;
    const auto& ui=submission.interfaceMesh;
    if(ui.vertexBytes||ui.indexCount){
        if(!ui.valid()||ui.strideBytes!=sizeof(HudVertex)||ui.vertexBytes+ui.indexCount*4>1024u*1024u){error="Interface geometry invalid or over budget";return false;}
        for(std::size_t i=0;i<ui.indexCount;++i)if(ui.indices[i]>=ui.vertexBytes/ui.strideBytes){error="Interface index outside vertices";return false;}
        if(!makeBuffer(uiVertices_,ui.vertexBytes,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,error)||!makeBuffer(uiIndices_,ui.indexCount*4,VK_BUFFER_USAGE_INDEX_BUFFER_BIT,error))return false;
        std::memcpy(uiVertices_.mapped,ui.vertices,ui.vertexBytes);std::memcpy(uiIndices_.mapped,ui.indices,ui.indexCount*4);
        for(const auto* buffer:{&uiVertices_,&uiIndices_})if(!buffer->coherent){VkMappedMemoryRange flush{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};flush.memory=buffer->memory;flush.size=VK_WHOLE_SIZE;
            if(vkFlushMappedMemoryRanges(device_,1,&flush)!=VK_SUCCESS){error="Interface memory flush failed";return false;}}
        uiIndexCount_=std::uint32_t(ui.indexCount);uiBytes_=ui.vertexBytes+ui.indexCount*4;
    }
    const std::size_t ib=mesh.indexCount*sizeof(std::uint32_t);
    if(mesh.vertexBytes>budget_/2 || ib>budget_/2-mesh.vertexBytes){
        error="Native mesh staging and device buffers exceed streaming budget";return false;
    }
    const auto vbCapacity=vertexDevice_.capacity,ibCapacity=indexDevice_.capacity;
    if(!makeBuffer(vertex_,std::max<std::size_t>(256u,mesh.vertexBytes),VK_BUFFER_USAGE_TRANSFER_SRC_BIT,error)||
       !makeBuffer(index_,std::max<std::size_t>(256u,ib),VK_BUFFER_USAGE_TRANSFER_SRC_BIT,error)||
       !makeDeviceBuffer(vertexDevice_,std::max<std::size_t>(256u,mesh.vertexBytes),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,error)||
       !makeDeviceBuffer(indexDevice_,std::max<std::size_t>(256u,ib),VK_BUFFER_USAGE_INDEX_BUFFER_BIT,error))return false;
    if(residentBytes()>budget_){error="Aligned native mesh allocations exceed streaming budget";return false;}
    pending_=uploads_.plan(mesh,vertexDevice_.capacity!=vbCapacity,indexDevice_.capacity!=ibCapacity);
    const auto vertexCount=mesh.vertexBytes/sizeof(SceneVertex);
    for(std::size_t i=pending_.indices.offset/sizeof(std::uint32_t);pending_.indices.bytes && i<mesh.indexCount;++i){
        if(mesh.indices[i]>=vertexCount || (mesh.resourceId && mesh.staticRevision && i<mesh.staticIndexCount &&
           mesh.indices[i]>=mesh.staticVertexBytes/sizeof(SceneVertex))){
            error="Native mesh index exceeds its vertex range";pending_={};return false;
        }
    }
    if(pending_.vertices.bytes)std::memcpy(static_cast<std::uint8_t*>(vertex_.mapped)+pending_.vertices.offset,
        static_cast<const std::uint8_t*>(mesh.vertices)+pending_.vertices.offset,pending_.vertices.bytes);
    if(pending_.indices.bytes)std::memcpy(static_cast<std::uint8_t*>(index_.mapped)+pending_.indices.offset,
        reinterpret_cast<const std::uint8_t*>(mesh.indices)+pending_.indices.offset,pending_.indices.bytes);
    if(pending_.vertices.bytes && !vertex_.coherent){
        VkMappedMemoryRange flush{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
        flush.memory=vertex_.memory;flush.size=VK_WHOLE_SIZE;
        if(vkFlushMappedMemoryRanges(device_,1,&flush)!=VK_SUCCESS){error="Vertex flush failed";return false;}
    }
    if(pending_.indices.bytes && !index_.coherent){
        VkMappedMemoryRange flush{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
        flush.memory=index_.memory;flush.size=VK_WHOLE_SIZE;
        if(vkFlushMappedMemoryRanges(device_,1,&flush)!=VK_SUCCESS){error="Index flush failed";return false;}
    }
    indices_=std::uint32_t(mesh.indexCount);
    auto frame=constants;
    if(submission.terrainMacro.valid()){frame.terrainMap[0]=submission.terrainMacro.originX;frame.terrainMap[1]=submission.terrainMacro.originZ;frame.terrainMap[2]=submission.terrainMacro.extent;frame.terrainMap[3]=1;}
    frame.viewport[2]=1.f/float(shadowSize_);
    frame.viewport[3]=(colorFormat_==VK_FORMAT_B8G8R8A8_SRGB || colorFormat_==VK_FORMAT_R8G8B8A8_SRGB)?0.f:1.f;
    std::memcpy(uniforms_.mapped,&frame,sizeof(frame));
    if(!uniforms_.coherent){
        VkMappedMemoryRange flush{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};flush.memory=uniforms_.memory;flush.size=VK_WHOLE_SIZE;
        if(vkFlushMappedMemoryRanges(device_,1,&flush)!=VK_SUCCESS){error="Frame uniform flush failed";return false;}
    }
    bytes=pending_.bytes();pendingMesh_=mesh;
    return true;
}
void VulkanScenePass::record(VkCommandBuffer cmd,std::uint32_t image,const GpuFrameConstants& constants){
    if(!ready_||image>=framebuffers_.size())return;
    std::array<VkBufferMemoryBarrier,2> barriers{};
    std::uint32_t count=0;
    auto copy=[&](const MeshUpdateRange& update,const HostBuffer& host,const DeviceBuffer& gpu,VkAccessFlags access){
        if(!update.bytes)return;
        VkBufferMemoryBarrier writable{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        writable.srcAccessMask=access;writable.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
        writable.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;writable.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        writable.buffer=gpu.buffer;writable.offset=update.offset;writable.size=update.bytes;
        vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,
            0,nullptr,1,&writable,0,nullptr);
        VkBufferCopy region{update.offset,update.offset,update.bytes};
        vkCmdCopyBuffer(cmd,host.buffer,gpu.buffer,1,&region);
        auto& barrier=barriers[count++];barrier.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=access;
        barrier.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer=gpu.buffer;barrier.offset=update.offset;barrier.size=update.bytes;
    };
    copy(pending_.vertices,vertex_,vertexDevice_,VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT);
    copy(pending_.indices,index_,indexDevice_,VK_ACCESS_INDEX_READ_BIT);
    if(count)vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,0,
        0,nullptr,count,barriers.data(),0,nullptr);
    uploads_.commit(pendingMesh_);pending_={};
    recordTextures(cmd);recordMacro(cmd);
    vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,layout_,0,1,&descriptor_,0,nullptr);
    const VkDeviceSize offset=0;
    vkCmdBindVertexBuffers(cmd,0,1,&vertexDevice_.buffer,&offset);
    vkCmdBindIndexBuffer(cmd,indexDevice_.buffer,0,VK_INDEX_TYPE_UINT32);
    VkClearValue shadowClear{};shadowClear.depthStencil={1.f,0};
    VkRenderPassBeginInfo shadowPass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    shadowPass.renderPass=shadowPass_;shadowPass.framebuffer=shadowFramebuffer_;shadowPass.renderArea={{0,0},{shadowSize_,shadowSize_}};
    shadowPass.clearValueCount=1;shadowPass.pClearValues=&shadowClear;
    vkCmdBeginRenderPass(cmd,&shadowPass,VK_SUBPASS_CONTENTS_INLINE);
    if(constants.sunColor[3]>.001f){vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,shadowPipeline_);vkCmdDrawIndexed(cmd,indices_,1,0,0,0);}
    vkCmdEndRenderPass(cmd);
    std::array<VkClearValue,2> clear{};
    clear[0].color={{.35f,.45f,.50f,1.f}};
    clear[1].depthStencil={1.f,0};
    VkRenderPassBeginInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rp.renderPass=pass_;rp.framebuffer=framebuffers_[image];rp.renderArea={{0,0},extent_};
    rp.clearValueCount=std::uint32_t(clear.size());rp.pClearValues=clear.data();
    vkCmdBeginRenderPass(cmd,&rp,VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,skyPipeline_);vkCmdDraw(cmd,3,1,0,0);
    vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_);
    vkCmdDrawIndexed(cmd,indices_,1,0,0,0);
    if(uiIndexCount_){vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,uiPipeline_);
        vkCmdBindVertexBuffers(cmd,0,1,&uiVertices_.buffer,&offset);vkCmdBindIndexBuffer(cmd,uiIndices_.buffer,0,VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd,uiIndexCount_,1,0,0,0);}
    vkCmdEndRenderPass(cmd);
}
} // namespace gz
