#include "VulkanScenePass.hpp"
#include "gz/Textures.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

namespace gz {
void VulkanScenePass::destroyImage(ImageResource& image)noexcept{
    if(device_){
        if(image.view)vkDestroyImageView(device_,image.view,nullptr);
        if(image.image)vkDestroyImage(device_,image.image,nullptr);
        if(image.memory)vkFreeMemory(device_,image.memory,nullptr);
    }
    image={};
}
bool VulkanScenePass::makeImage(ImageResource& out,VkFormat format,std::uint32_t width,std::uint32_t height,
    std::uint32_t levels,std::uint32_t layers,VkSampleCountFlagBits samples,VkImageUsageFlags usage,
    VkImageAspectFlags aspect,std::string& error){
    VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ci.imageType=VK_IMAGE_TYPE_2D;ci.format=format;ci.extent={width,height,1};ci.mipLevels=levels;
    ci.arrayLayers=layers;ci.samples=samples;ci.tiling=VK_IMAGE_TILING_OPTIMAL;
    ci.usage=usage;ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    if(vkCreateImage(device_,&ci,nullptr,&out.image)!=VK_SUCCESS){error="Native material/render image creation failed";return false;}
    VkMemoryRequirements need{};vkGetImageMemoryRequirements(device_,out.image,&need);
    std::uint32_t type=0;
    if(!allocMemory(need.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,type)){error="No device-local image memory";return false;}
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.allocationSize=need.size;ai.memoryTypeIndex=type;
    if(vkAllocateMemory(device_,&ai,nullptr,&out.memory)!=VK_SUCCESS ||
        vkBindImageMemory(device_,out.image,out.memory,0)!=VK_SUCCESS){error="Native image memory allocation failed";return false;}
    out.allocationBytes=need.size;
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image=out.image;vi.viewType=layers>1?VK_IMAGE_VIEW_TYPE_2D_ARRAY:VK_IMAGE_VIEW_TYPE_2D;vi.format=format;
    vi.subresourceRange={aspect,0,levels,0,layers};
    if(vkCreateImageView(device_,&vi,nullptr,&out.view)!=VK_SUCCESS){error="Native image view creation failed";return false;}
    return true;
}
bool VulkanScenePass::makeMaterials(std::string& error){
    constexpr std::uint32_t resolution=512,levels=10,layers=17;
    TextureLibrary library;
    materialCount_=std::uint32_t(library.load("assets/textures/runtime",resolution));
    if(materialCount_!=layers-1){error="Native renderer requires all 16 packaged 512px materials";return false;}
    std::vector<std::uint8_t> upload;
    textureCopies_.clear();
    for(std::uint32_t layer=0;layer<layers;++layer){
        std::vector<std::uint8_t> pixels(resolution*resolution*4);
        for(std::uint32_t y=0;y<resolution;++y)for(std::uint32_t x=0;x<resolution;++x){
            const auto rgb=layer?library.sample(TextureMaterial(layer),(x+.5f)/resolution,(y+.5f)/resolution):0x596d43u;
            const auto i=(y*resolution+x)*4u;
            pixels[i]=std::uint8_t(rgb>>16);pixels[i+1]=std::uint8_t(rgb>>8);pixels[i+2]=std::uint8_t(rgb);pixels[i+3]=255;
        }
        std::uint32_t side=resolution;
        for(std::uint32_t level=0;level<levels;++level){
            VkBufferImageCopy copy{};copy.bufferOffset=upload.size();
            copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,level,layer,1};copy.imageExtent={side,side,1};
            textureCopies_.push_back(copy);upload.insert(upload.end(),pixels.begin(),pixels.end());
            if(side==1)break;
            const auto smaller=side/2;
            std::vector<std::uint8_t> next(smaller*smaller*4);
            for(std::uint32_t y=0;y<smaller;++y)for(std::uint32_t x=0;x<smaller;++x){
                for(std::uint32_t c=0;c<4;++c){
                    const auto a=((2*y)*side+2*x)*4+c;
                    // Average decoded sRGB before encoding the next mip level.
                    if(c==3){next[(y*smaller+x)*4+c]=255;continue;}
                    float sum=0;
                    for(const auto index:{a,a+4,a+side*4,a+side*4+4}){
                        const float v=pixels[index]/255.f;
                        sum+=v<=.04045f?v/12.92f:std::pow((v+.055f)/1.055f,2.4f);
                    }
                    const float v=sum*.25f;
                    next[(y*smaller+x)*4+c]=std::uint8_t(std::clamp((v<=.0031308f?v*12.92f:1.055f*std::pow(v,1.f/2.4f)-.055f)*255.f+.5f,0.f,255.f));
                }
            }
            pixels=std::move(next);side=smaller;
        }
    }
    if(!makeImage(materials_,VK_FORMAT_R8G8B8A8_SRGB,resolution,resolution,levels,layers,VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_COLOR_BIT,error) ||
       !makeBuffer(textureStaging_,upload.size(),VK_BUFFER_USAGE_TRANSFER_SRC_BIT,error))return false;
    std::memcpy(textureStaging_.mapped,upload.data(),upload.size());
    if(!textureStaging_.coherent){
        VkMappedMemoryRange flush{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};flush.memory=textureStaging_.memory;flush.size=VK_WHOLE_SIZE;
        if(vkFlushMappedMemoryRanges(device_,1,&flush)!=VK_SUCCESS){error="Material upload flush failed";return false;}
    }
    VkPhysicalDeviceFeatures features{};vkGetPhysicalDeviceFeatures(physical_,&features);
    VkPhysicalDeviceProperties properties{};vkGetPhysicalDeviceProperties(physical_,&properties);
    VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampler.magFilter=sampler.minFilter=VK_FILTER_LINEAR;sampler.mipmapMode=VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler.addressModeU=sampler.addressModeV=sampler.addressModeW=VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler.maxLod=float(levels-1);sampler.anisotropyEnable=features.samplerAnisotropy;
    sampler.maxAnisotropy=features.samplerAnisotropy?std::min(8.f,properties.limits.maxSamplerAnisotropy):1.f;
    if(vkCreateSampler(device_,&sampler,nullptr,&materialSampler_)!=VK_SUCCESS){error="Material sampler creation failed";return false;}
    texturesPending_=true;return true;
}
bool VulkanScenePass::makeShadow(std::string& error){
    VkFormat shadowFormat=VK_FORMAT_UNDEFINED;
    for(const auto candidate:{VK_FORMAT_D32_SFLOAT,VK_FORMAT_D16_UNORM}){
        VkFormatProperties props{};vkGetPhysicalDeviceFormatProperties(physical_,candidate,&props);
        const auto required=VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
        if((props.optimalTilingFeatures&required)==required){shadowFormat=candidate;break;}
    }
    if(shadowFormat==VK_FORMAT_UNDEFINED){error="No sampleable shadow depth format";return false;}
    if(!makeImage(shadow_,shadowFormat,shadowSize_,shadowSize_,1,1,VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_DEPTH_BIT,error))return false;
    VkAttachmentDescription attachment{};attachment.format=shadowFormat;attachment.samples=VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;attachment.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;attachment.finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    VkAttachmentReference depth{0,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub{};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;sub.pDepthStencilAttachment=&depth;
    std::array<VkSubpassDependency,2> deps{};
    deps[0].srcSubpass=VK_SUBPASS_EXTERNAL;deps[0].dstSubpass=0;
    deps[0].srcStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;deps[0].srcAccessMask=VK_ACCESS_SHADER_READ_BIT;
    deps[0].dstStageMask=VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    deps[0].dstAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    deps[1].srcSubpass=0;deps[1].dstSubpass=VK_SUBPASS_EXTERNAL;
    deps[1].srcStageMask=VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    deps[1].srcAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    deps[1].dstStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;deps[1].dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
    VkRenderPassCreateInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    pass.attachmentCount=1;pass.pAttachments=&attachment;pass.subpassCount=1;pass.pSubpasses=&sub;
    pass.dependencyCount=2;pass.pDependencies=deps.data();
    if(vkCreateRenderPass(device_,&pass,nullptr,&shadowPass_)!=VK_SUCCESS){error="Shadow render pass creation failed";return false;}
    VkFramebufferCreateInfo fb{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fb.renderPass=shadowPass_;fb.attachmentCount=1;fb.pAttachments=&shadow_.view;
    fb.width=fb.height=shadowSize_;fb.layers=1;
    if(vkCreateFramebuffer(device_,&fb,nullptr,&shadowFramebuffer_)!=VK_SUCCESS){error="Shadow framebuffer creation failed";return false;}
    VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampler.magFilter=sampler.minFilter=VK_FILTER_NEAREST;sampler.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler.addressModeU=sampler.addressModeV=sampler.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if(vkCreateSampler(device_,&sampler,nullptr,&shadowSampler_)!=VK_SUCCESS){error="Shadow sampler creation failed";return false;}
    return true;
}
bool VulkanScenePass::makeDescriptors(std::string& error){
    if(!makeBuffer(uniforms_,sizeof(GpuFrameConstants),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,error))return false;
    const std::array<VkDescriptorSetLayoutBinding,3> bindings{{
        {0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,nullptr},
        {1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr},
        {2,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr}
    }};
    VkDescriptorSetLayoutCreateInfo layout{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layout.bindingCount=3;layout.pBindings=bindings.data();
    if(vkCreateDescriptorSetLayout(device_,&layout,nullptr,&descriptorLayout_)!=VK_SUCCESS){error="Native descriptor layout creation failed";return false;}
    const std::array<VkDescriptorPoolSize,2> sizes{{{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,2}}};
    VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};pool.maxSets=1;pool.poolSizeCount=2;pool.pPoolSizes=sizes.data();
    if(vkCreateDescriptorPool(device_,&pool,nullptr,&descriptorPool_)!=VK_SUCCESS){error="Native descriptor pool creation failed";return false;}
    VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocation.descriptorPool=descriptorPool_;allocation.descriptorSetCount=1;allocation.pSetLayouts=&descriptorLayout_;
    if(vkAllocateDescriptorSets(device_,&allocation,&descriptor_)!=VK_SUCCESS){error="Native descriptor allocation failed";return false;}
    VkDescriptorBufferInfo buffer{uniforms_.buffer,0,sizeof(GpuFrameConstants)};
    const std::array<VkDescriptorImageInfo,2> images{{
        {materialSampler_,materials_.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {shadowSampler_,shadow_.view,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL}
    }};
    std::array<VkWriteDescriptorSet,3> writes{};
    for(std::uint32_t i=0;i<3;++i){
        auto& w=writes[i];w.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;w.dstSet=descriptor_;w.dstBinding=i;w.descriptorCount=1;
        w.descriptorType=i?VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        if(i)w.pImageInfo=&images[i-1];else w.pBufferInfo=&buffer;
    }
    vkUpdateDescriptorSets(device_,3,writes.data(),0,nullptr);return true;
}
void VulkanScenePass::recordTextures(VkCommandBuffer cmd){
    if(!texturesPending_)return;
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    barrier.image=materials_.image;barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,10,0,17};
    barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
    vkCmdCopyBufferToImage(cmd,textureStaging_.buffer,materials_.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        std::uint32_t(textureCopies_.size()),textureCopies_.data());
    barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
    texturesPending_=false;
}
} // namespace gz
