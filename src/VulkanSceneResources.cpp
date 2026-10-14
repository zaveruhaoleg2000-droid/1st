#include "VulkanScenePass.hpp"
#include "gz/Textures.hpp"
#include "gz/Sprites.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
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
    constexpr std::uint32_t resolution=512,levels=10,layers=std::uint32_t(TextureMaterial::Count);
    TextureLibrary library;
    materialCount_=std::uint32_t(library.load("assets/textures/runtime",resolution));
    if(materialCount_!=layers-1){error="Native renderer requires all packaged 512px materials";return false;}
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
    sampler.maxLod=VK_LOD_CLAMP_NONE;sampler.anisotropyEnable=features.samplerAnisotropy;
    sampler.maxAnisotropy=features.samplerAnisotropy?std::min(8.f,properties.limits.maxSamplerAnisotropy):1.f;
    if(vkCreateSampler(device_,&sampler,nullptr,&materialSampler_)!=VK_SUCCESS){error="Material sampler creation failed";return false;}
    TextureLibrary normals;if(normals.load("assets/textures/normals",256)!=layers-1){error="Procedural normal maps missing";return false;}
    std::vector<std::uint8_t> normalData;normalCopies_.clear();
    for(std::uint32_t layer=0;layer<layers;++layer){std::vector<std::uint8_t> pixels(256*256*4);
        for(unsigned y=0;y<256;++y)for(unsigned x=0;x<256;++x){auto rgb=layer?normals.sample(TextureMaterial(layer),(x+.5f)/256,(y+.5f)/256):0x8080ff;
            auto i=(y*256+x)*4;pixels[i]=std::uint8_t(rgb>>16);pixels[i+1]=std::uint8_t(rgb>>8);pixels[i+2]=std::uint8_t(rgb);pixels[i+3]=255;}
        unsigned side=256;
        for(unsigned level=0;level<9;++level){VkBufferImageCopy copy{};copy.bufferOffset=normalData.size();copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,level,layer,1};copy.imageExtent={side,side,1};normalCopies_.push_back(copy);normalData.insert(normalData.end(),pixels.begin(),pixels.end());
            if(side==1)break;
            unsigned small=side/2;std::vector<std::uint8_t> next(small*small*4);
            for(unsigned y=0;y<small;++y)for(unsigned x=0;x<small;++x){float n[3]{};for(unsigned oy=0;oy<2;++oy)for(unsigned ox=0;ox<2;++ox)for(unsigned c=0;c<3;++c)n[c]+=pixels[((2*y+oy)*side+2*x+ox)*4+c]/127.5f-1.f;
                const float len=std::max(.001f,std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]));for(unsigned c=0;c<3;++c)next[(y*small+x)*4+c]=std::uint8_t(std::clamp((n[c]/len*.5f+.5f)*255.f,0.f,255.f));next[(y*small+x)*4+3]=255;}
            pixels=std::move(next);side=small;
        }
    }
    if(!makeImage(normals_,VK_FORMAT_R8G8B8A8_UNORM,256,256,9,layers,VK_SAMPLE_COUNT_1_BIT,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_COLOR_BIT,error)||!makeBuffer(normalStaging_,normalData.size(),VK_BUFFER_USAGE_TRANSFER_SRC_BIT,error))return false;
    std::memcpy(normalStaging_.mapped,normalData.data(),normalData.size());
    if(!normalStaging_.coherent){VkMappedMemoryRange flush{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};flush.memory=normalStaging_.memory;flush.size=VK_WHOLE_SIZE;if(vkFlushMappedMemoryRanges(device_,1,&flush)!=VK_SUCCESS)return false;}
    texturesPending_=true;return true;
}
bool VulkanScenePass::makeSprites(std::string& error){
    SpriteAtlas atlas;if(!atlas.load()){error="Procedural RGBA sprite atlas missing";return false;}
    std::vector<std::uint8_t> upload;spriteCopies_.clear();
    for(unsigned layer=0;layer<atlas.layers;++layer){std::vector<std::uint8_t> pixels(atlas.pixels.begin()+layer*256*256*4,atlas.pixels.begin()+(layer+1)*256*256*4);unsigned side=256;
        for(unsigned level=0;level<9;++level){VkBufferImageCopy copy{};copy.bufferOffset=upload.size();copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,level,layer,1};copy.imageExtent={side,side,1};spriteCopies_.push_back(copy);upload.insert(upload.end(),pixels.begin(),pixels.end());
            if(side==1)break;unsigned small=side/2;std::vector<std::uint8_t> next(small*small*4);
            for(unsigned y=0;y<small;++y)for(unsigned x=0;x<small;++x){float colors[3]{},alpha=0;
                for(unsigned oy=0;oy<2;++oy)for(unsigned ox=0;ox<2;++ox){auto i=((2*y+oy)*side+2*x+ox)*4;float a=pixels[i+3]/255.f;alpha+=a;for(unsigned c=0;c<3;++c){float v=pixels[i+c]/255.f;colors[c]+=(v<=.04045f?v/12.92f:std::pow((v+.055f)/1.055f,2.4f))*a;}}
                for(unsigned c=0;c<3;++c){float v=alpha>0?colors[c]/alpha:0;next[(y*small+x)*4+c]=std::uint8_t(std::clamp((v<=.0031308f?v*12.92f:1.055f*std::pow(v,1/2.4f)-.055f)*255,0.f,255.f));}next[(y*small+x)*4+3]=std::uint8_t(alpha*.25f*255);}
            pixels=std::move(next);side=small;
        }
    }
    if(!makeImage(sprites_,VK_FORMAT_R8G8B8A8_SRGB,256,256,9,atlas.layers,VK_SAMPLE_COUNT_1_BIT,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_COLOR_BIT,error)||!makeBuffer(spriteStaging_,upload.size(),VK_BUFFER_USAGE_TRANSFER_SRC_BIT,error))return false;
    std::memcpy(spriteStaging_.mapped,upload.data(),upload.size());if(!spriteStaging_.coherent){VkMappedMemoryRange f{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};f.memory=spriteStaging_.memory;f.size=VK_WHOLE_SIZE;if(vkFlushMappedMemoryRanges(device_,1,&f)!=VK_SUCCESS)return false;}
    VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};sampler.magFilter=sampler.minFilter=VK_FILTER_LINEAR;sampler.mipmapMode=VK_SAMPLER_MIPMAP_MODE_LINEAR;sampler.maxLod=8;sampler.addressModeU=sampler.addressModeV=sampler.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if(vkCreateSampler(device_,&sampler,nullptr,&spriteSampler_)!=VK_SUCCESS){error="Sprite sampler creation failed";return false;}return true;
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
bool VulkanScenePass::makeInterface(std::string& error){
    std::ifstream in("assets/ui/glyphs.pgm",std::ios::binary);std::string magic;int w=0,h=0,maximum=0;
    if(!(in>>magic>>w>>h>>maximum)||magic!="P5"||maximum!=255||w<=0||h<=0||w>2048||h>2048){error="Interface glyph atlas missing or invalid";return false;}
    in.get();std::vector<std::uint8_t> pixels(std::size_t(w)*h);in.read(reinterpret_cast<char*>(pixels.data()),std::streamsize(pixels.size()));
    if(!in){error="Interface atlas truncated";return false;}
    glyphExtent_={std::uint32_t(w),std::uint32_t(h)};
    if(!makeImage(glyphs_,VK_FORMAT_R8_UNORM,std::uint32_t(w),std::uint32_t(h),1,1,VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_COLOR_BIT,error)||!makeBuffer(glyphStaging_,pixels.size(),VK_BUFFER_USAGE_TRANSFER_SRC_BIT,error))return false;
    std::memcpy(glyphStaging_.mapped,pixels.data(),pixels.size());
    if(!glyphStaging_.coherent){VkMappedMemoryRange flush{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};flush.memory=glyphStaging_.memory;flush.size=VK_WHOLE_SIZE;
        if(vkFlushMappedMemoryRanges(device_,1,&flush)!=VK_SUCCESS){error="Interface atlas flush failed";return false;}}
    VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};sampler.magFilter=sampler.minFilter=VK_FILTER_LINEAR;
    sampler.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;sampler.maxLod=0;sampler.addressModeU=sampler.addressModeV=sampler.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if(vkCreateSampler(device_,&sampler,nullptr,&glyphSampler_)!=VK_SUCCESS){error="HUD sampler creation failed";return false;}
    return true;
}
bool VulkanScenePass::prepareMacro(const TerrainMacroView& data,std::string& error){
    macroBytes_=0;
    if(macro_.image&&(!data.valid()||(data.revision!=0&&data.revision==macroRevision_)))return true;
    const std::uint32_t resolution=data.valid()?data.resolution:1;
    std::size_t expected=0;std::uint32_t levels=0;
    for(auto side=resolution;side;side/=2){expected+=std::size_t(side)*side*4;++levels;}
    if(data.valid()&&data.bytes!=expected){error="Terrain macro mip data size invalid";return false;}
    destroyImage(macro_);destroyBuffer(macroStaging_);macroCopies_.clear();
    if(!makeImage(macro_,VK_FORMAT_R8G8B8A8_SRGB,resolution,resolution,levels,1,VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_COLOR_BIT,error)||
       !makeBuffer(macroStaging_,expected,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,error))return false;
    if(data.valid())std::memcpy(macroStaging_.mapped,data.pixels,expected);else std::memset(macroStaging_.mapped,255,expected);
    if(!macroStaging_.coherent){VkMappedMemoryRange flush{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};flush.memory=macroStaging_.memory;flush.size=VK_WHOLE_SIZE;
        if(vkFlushMappedMemoryRanges(device_,1,&flush)!=VK_SUCCESS){error="Terrain macro flush failed";return false;}}
    std::size_t offset=0;std::uint32_t level=0;
    for(auto side=resolution;side;side/=2){VkBufferImageCopy copy{};copy.bufferOffset=offset;copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,level++,0,1};copy.imageExtent={side,side,1};
        macroCopies_.push_back(copy);offset+=std::size_t(side)*side*4;}
    macroRevision_=data.valid()?data.revision:0;macroResolution_=data.valid()?resolution:0;
    macroPending_=true;macroBytes_=data.valid()?expected:0;
    if(descriptor_){
        VkDescriptorImageInfo image{materialSampler_,macro_.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};write.dstSet=descriptor_;write.dstBinding=4;write.descriptorCount=1;write.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;write.pImageInfo=&image;
        vkUpdateDescriptorSets(device_,1,&write,0,nullptr);
    }return true;
}
void VulkanScenePass::recordMacro(VkCommandBuffer cmd){
    if(!macroPending_)return;
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};barrier.image=macro_.image;
    barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,std::uint32_t(macroCopies_.size()),0,1};
    barrier.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
    vkCmdCopyBufferToImage(cmd,macroStaging_.buffer,macro_.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,std::uint32_t(macroCopies_.size()),macroCopies_.data());
    barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,1,&barrier);macroPending_=false;
}
bool VulkanScenePass::makeDescriptors(std::string& error){
    if(!makeBuffer(uniforms_,sizeof(GpuFrameConstants),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,error))return false;
    const std::array<VkDescriptorSetLayoutBinding,7> bindings{{
        {0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,nullptr},
        {1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr},
        {2,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr},
        {3,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr},
        {4,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr},
        {5,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr},
        {6,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr}
    }};
    VkDescriptorSetLayoutCreateInfo layout{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layout.bindingCount=7;layout.pBindings=bindings.data();
    if(vkCreateDescriptorSetLayout(device_,&layout,nullptr,&descriptorLayout_)!=VK_SUCCESS){error="Native descriptor layout creation failed";return false;}
    const std::array<VkDescriptorPoolSize,2> sizes{{{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,6}}};
    VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};pool.maxSets=1;pool.poolSizeCount=2;pool.pPoolSizes=sizes.data();
    if(vkCreateDescriptorPool(device_,&pool,nullptr,&descriptorPool_)!=VK_SUCCESS){error="Native descriptor pool creation failed";return false;}
    VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocation.descriptorPool=descriptorPool_;allocation.descriptorSetCount=1;allocation.pSetLayouts=&descriptorLayout_;
    if(vkAllocateDescriptorSets(device_,&allocation,&descriptor_)!=VK_SUCCESS){error="Native descriptor allocation failed";return false;}
    VkDescriptorBufferInfo buffer{uniforms_.buffer,0,sizeof(GpuFrameConstants)};
    const std::array<VkDescriptorImageInfo,6> images{{
        {materialSampler_,materials_.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {shadowSampler_,shadow_.view,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL},
        {glyphSampler_,glyphs_.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {materialSampler_,macro_.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {materialSampler_,normals_.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {spriteSampler_,sprites_.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}
    }};
    std::array<VkWriteDescriptorSet,7> writes{};
    for(std::uint32_t i=0;i<7;++i){
        auto& w=writes[i];w.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;w.dstSet=descriptor_;w.dstBinding=i;w.descriptorCount=1;
        w.descriptorType=i?VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        if(i)w.pImageInfo=&images[i-1];else w.pBufferInfo=&buffer;
    }
    vkUpdateDescriptorSets(device_,7,writes.data(),0,nullptr);return true;
}
void VulkanScenePass::recordTextures(VkCommandBuffer cmd){
    if(!texturesPending_)return;
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    barrier.image=materials_.image;barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,10,0,std::uint32_t(TextureMaterial::Count)};
    barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
    vkCmdCopyBufferToImage(cmd,textureStaging_.buffer,materials_.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        std::uint32_t(textureCopies_.size()),textureCopies_.data());
    barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
    barrier.image=normals_.image;barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,9,0,std::uint32_t(TextureMaterial::Count)};
    barrier.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;barrier.srcAccessMask=0;barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
    vkCmdCopyBufferToImage(cmd,normalStaging_.buffer,normals_.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,std::uint32_t(normalCopies_.size()),normalCopies_.data());
    barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
    barrier.image=glyphs_.image;barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    barrier.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;barrier.srcAccessMask=0;barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
    VkBufferImageCopy glyphCopy{};glyphCopy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};glyphCopy.imageExtent={glyphExtent_.width,glyphExtent_.height,1};
    vkCmdCopyBufferToImage(cmd,glyphStaging_.buffer,glyphs_.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&glyphCopy);
    barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
    barrier.image=sprites_.image;barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,9,0,unsigned(SpriteKind::Count)};barrier.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;barrier.srcAccessMask=0;barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);vkCmdCopyBufferToImage(cmd,spriteStaging_.buffer,sprites_.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,unsigned(spriteCopies_.size()),spriteCopies_.data());
    barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
    texturesPending_=false;
}
} // namespace gz
