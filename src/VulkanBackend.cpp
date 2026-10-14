// Vulkan WSI backend v1. Vulkan owns the swapchain, staging memory,
// synchronization and presentation. The scene is still rasterized to the
// immutable FramePixels CPU buffer; migrating scene draw calls to Vulkan
// pipelines is deliberately a separate, measurable migration step.
#if defined(_WIN32)
# define NOMINMAX
# define VK_USE_PLATFORM_WIN32_KHR
# include <windows.h>
#elif defined(__linux__)
# define VK_USE_PLATFORM_XLIB_KHR
# include <X11/Xlib.h>
# undef None // X11's macro conflicts with the scoped TextureMaterial::None enum.
#endif
#include <vulkan/vulkan.h>
#include "gz/RenderBackend.hpp"
#include "gz/RenderGraph.hpp"
#include "VulkanScenePass.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace gz {
namespace {
bool hasDeviceExtension(VkPhysicalDevice gpu,const char* required){
    std::uint32_t count=0;
    if(vkEnumerateDeviceExtensionProperties(gpu,nullptr,&count,nullptr)!=VK_SUCCESS)return false;
    std::vector<VkExtensionProperties> ext(count);
    if(vkEnumerateDeviceExtensionProperties(gpu,nullptr,&count,ext.data())!=VK_SUCCESS)return false;
    for(const auto& e:ext)if(std::strcmp(e.extensionName,required)==0)return true;
    return false;
}
bool hasInstanceExtension(const char* required){
    std::uint32_t count=0;
    if(vkEnumerateInstanceExtensionProperties(nullptr,&count,nullptr)!=VK_SUCCESS)return false;
    std::vector<VkExtensionProperties> ext(count);
    if(vkEnumerateInstanceExtensionProperties(nullptr,&count,ext.data())!=VK_SUCCESS)return false;
    for(const auto& e:ext)if(std::strcmp(e.extensionName,required)==0)return true;
    return false;
}
class VulkanBackend final:public IRenderBackend {
    NativeRenderWindow native_{};
    GraphicsLimits limits_{};
    mutable RenderTelemetry telemetry_{};
    std::atomic<std::uint64_t> validationErrors_{0};
    VkDebugUtilsMessengerEXT debugMessenger_=VK_NULL_HANDLE;
    VkInstance instance_=VK_NULL_HANDLE;
    VkSurfaceKHR surface_=VK_NULL_HANDLE;
    VkPhysicalDevice gpu_=VK_NULL_HANDLE;
    VkDevice device_=VK_NULL_HANDLE;
    VkQueue queue_=VK_NULL_HANDLE;
    std::uint32_t queueFamily_=0;
    VkSwapchainKHR swapchain_=VK_NULL_HANDLE;
    VkFormat format_=VK_FORMAT_UNDEFINED;
    VkExtent2D extent_{};
    std::vector<VkImage> images_;
    std::vector<bool> previouslyPresented_;
    VkCommandPool pool_=VK_NULL_HANDLE;
    VkCommandBuffer command_=VK_NULL_HANDLE;
    VkSemaphore acquired_=VK_NULL_HANDLE;
    std::vector<VkSemaphore> finished_;
    VkFence fence_=VK_NULL_HANDLE;
    VkBuffer staging_=VK_NULL_HANDLE;
    VkDeviceMemory memory_=VK_NULL_HANDLE;
    void* mapped_=nullptr;
    VkDeviceSize stageBytes_=0;
    bool coherent_=false;
    bool initialized_=false;
    VulkanScenePass nativeScene_{};
    bool nativeRenderTarget_=false;
    static VKAPI_ATTR VkBool32 VKAPI_CALL validationMessage(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT,const VkDebugUtilsMessengerCallbackDataEXT* data,void* user){
        if(severity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
            static_cast<VulkanBackend*>(user)->validationErrors_.fetch_add(1,std::memory_order_relaxed);
        std::cerr<<"Vulkan validation "<<((severity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)?"ERROR":"WARNING")
            <<": "<<data->pMessage<<'\n';
        return VK_FALSE;
    }
    void releaseStaging() noexcept {
        if(device_){
            if(memory_&&mapped_)vkUnmapMemory(device_,memory_);
            mapped_=nullptr;
            if(staging_)vkDestroyBuffer(device_,staging_,nullptr);
            if(memory_)vkFreeMemory(device_,memory_,nullptr);
        }
        staging_=VK_NULL_HANDLE;memory_=VK_NULL_HANDLE;stageBytes_=0;
    }
    void destroy() noexcept {
        if(device_)vkDeviceWaitIdle(device_);
        nativeScene_.destroy();
        releaseStaging();
        if(device_){
            if(fence_)vkDestroyFence(device_,fence_,nullptr);
            if(acquired_)vkDestroySemaphore(device_,acquired_,nullptr);
            for(auto semaphore:finished_)if(semaphore)vkDestroySemaphore(device_,semaphore,nullptr);
            if(pool_)vkDestroyCommandPool(device_,pool_,nullptr);
            if(swapchain_)vkDestroySwapchainKHR(device_,swapchain_,nullptr);
            vkDestroyDevice(device_,nullptr);
        }
        if(instance_){
            if(surface_)vkDestroySurfaceKHR(instance_,surface_,nullptr);
            if(debugMessenger_){
                const auto destroyDebug=reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance_,"vkDestroyDebugUtilsMessengerEXT"));
                if(destroyDebug)destroyDebug(instance_,debugMessenger_,nullptr);
            }
            vkDestroyInstance(instance_,nullptr);
        }
        instance_=VK_NULL_HANDLE;surface_=VK_NULL_HANDLE;gpu_=VK_NULL_HANDLE;
        device_=VK_NULL_HANDLE;queue_=VK_NULL_HANDLE;swapchain_=VK_NULL_HANDLE;
        fence_=VK_NULL_HANDLE;acquired_=VK_NULL_HANDLE;finished_.clear();pool_=VK_NULL_HANDLE;
        debugMessenger_=VK_NULL_HANDLE;
        command_=VK_NULL_HANDLE;initialized_=false;
        images_.clear();previouslyPresented_.clear();
    }
    bool createStaging(std::string& error){
        const VkDeviceSize required=VkDeviceSize(extent_.width)*extent_.height*4;
        if(required>limits_.streamingBudgetBytes){
            error="Vulkan staging frame exceeds streaming memory budget";return false;
        }
        if(staging_ && required<=stageBytes_)return true;
        releaseStaging();
        VkBufferCreateInfo bc{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bc.size=required;bc.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bc.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        if(vkCreateBuffer(device_,&bc,nullptr,&staging_)!=VK_SUCCESS){error="vkCreateBuffer staging failed";return false;}
        VkMemoryRequirements needs{};vkGetBufferMemoryRequirements(device_,staging_,&needs);
        VkPhysicalDeviceMemoryProperties mp{};vkGetPhysicalDeviceMemoryProperties(gpu_,&mp);
        std::uint32_t memIndex=UINT32_MAX;
        // Prefer coherent host memory to avoid flushing on every frame.
        for(std::uint32_t pass=0;pass<2 && memIndex==UINT32_MAX;++pass){
            auto flags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                (pass==0?VK_MEMORY_PROPERTY_HOST_COHERENT_BIT:VkMemoryPropertyFlags(0));
            for(std::uint32_t i=0;i<mp.memoryTypeCount;++i)
                if((needs.memoryTypeBits&(1u<<i)) && (mp.memoryTypes[i].propertyFlags&flags)==flags){memIndex=i;break;}
        }
        if(memIndex==UINT32_MAX){error="No host-visible Vulkan staging memory";return false;}
        coherent_=(mp.memoryTypes[memIndex].propertyFlags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)!=0;
        VkMemoryAllocateInfo ma{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ma.allocationSize=needs.size;ma.memoryTypeIndex=memIndex;
        if(vkAllocateMemory(device_,&ma,nullptr,&memory_)!=VK_SUCCESS){error="vkAllocateMemory staging failed";return false;}
        if(vkBindBufferMemory(device_,staging_,memory_,0)!=VK_SUCCESS ||
            vkMapMemory(device_,memory_,0,VK_WHOLE_SIZE,0,&mapped_)!=VK_SUCCESS){
            error="Vulkan buffer binding or mapping failed";return false;
        }
        stageBytes_=required;
        return true;
    }
    bool createSwapchain(std::uint32_t requestedWidth,std::uint32_t requestedHeight,std::string& error){
        VkSurfaceCapabilitiesKHR caps{};
        if(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu_,surface_,&caps)!=VK_SUCCESS){error="Surface capabilities unavailable";return false;}
        if(!(caps.supportedUsageFlags&VK_IMAGE_USAGE_TRANSFER_DST_BIT)){
            error="Surface does not support VK_IMAGE_USAGE_TRANSFER_DST_BIT";return false;
        }
        std::uint32_t n=0;
        if(vkGetPhysicalDeviceSurfaceFormatsKHR(gpu_,surface_,&n,nullptr)!=VK_SUCCESS||n==0){error="No surface formats";return false;}
        std::vector<VkSurfaceFormatKHR> available(n);
        if(vkGetPhysicalDeviceSurfaceFormatsKHR(gpu_,surface_,&n,available.data())!=VK_SUCCESS){error="Surface formats query failed";return false;}
        VkSurfaceFormatKHR chosen=available.front();
        for(const auto& candidate:available){
            if(candidate.format==VK_FORMAT_B8G8R8A8_UNORM || candidate.format==VK_FORMAT_R8G8B8A8_UNORM ||
               candidate.format==VK_FORMAT_B8G8R8A8_SRGB || candidate.format==VK_FORMAT_R8G8B8A8_SRGB){chosen=candidate;break;}
        }
        if(chosen.format==VK_FORMAT_UNDEFINED){chosen.format=VK_FORMAT_B8G8R8A8_UNORM;chosen.colorSpace=VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;}
        if(chosen.format!=VK_FORMAT_B8G8R8A8_UNORM && chosen.format!=VK_FORMAT_R8G8B8A8_UNORM &&
           chosen.format!=VK_FORMAT_B8G8R8A8_SRGB && chosen.format!=VK_FORMAT_R8G8B8A8_SRGB){
            error="No suitable 8-bit swapchain format";return false;
        }
        VkExtent2D newExtent{std::clamp(requestedWidth,caps.minImageExtent.width,caps.maxImageExtent.width),
                             std::clamp(requestedHeight,caps.minImageExtent.height,caps.maxImageExtent.height)};
        if(caps.currentExtent.width!=UINT32_MAX)newExtent=caps.currentExtent;
        if(!newExtent.width||!newExtent.height){error="Minimized surface";return false;}
        std::uint32_t imageCount=std::max(2u,caps.minImageCount+1u);
        if(caps.maxImageCount)imageCount=std::min(imageCount,caps.maxImageCount);
        VkSwapchainCreateInfoKHR sc{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        sc.surface=surface_;sc.minImageCount=imageCount;sc.imageFormat=chosen.format;sc.imageColorSpace=chosen.colorSpace;
        nativeRenderTarget_=(caps.supportedUsageFlags&VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)!=0;
        sc.imageExtent=newExtent;sc.imageArrayLayers=1;
        sc.imageUsage=VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                      (nativeRenderTarget_?VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT:VkImageUsageFlags(0));
        sc.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE;
        sc.preTransform=(caps.supportedTransforms&VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)?
            VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR:caps.currentTransform;
        sc.compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        if(!(caps.supportedCompositeAlpha&sc.compositeAlpha)){
            for(auto a:{VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR})
                if(caps.supportedCompositeAlpha&a){sc.compositeAlpha=a;break;}
        }
        sc.presentMode=VK_PRESENT_MODE_FIFO_KHR;sc.clipped=VK_TRUE;sc.oldSwapchain=swapchain_;
        VkSwapchainKHR newSwapchain=VK_NULL_HANDLE;
        if(vkCreateSwapchainKHR(device_,&sc,nullptr,&newSwapchain)!=VK_SUCCESS){error="vkCreateSwapchainKHR failed";return false;}
        std::uint32_t actual=0;
        if(vkGetSwapchainImagesKHR(device_,newSwapchain,&actual,nullptr)!=VK_SUCCESS||!actual){
            vkDestroySwapchainKHR(device_,newSwapchain,nullptr);error="Swapchain has no images";return false;
        }
        std::vector<VkImage> newImages(actual);
        if(vkGetSwapchainImagesKHR(device_,newSwapchain,&actual,newImages.data())!=VK_SUCCESS){
            vkDestroySwapchainKHR(device_,newSwapchain,nullptr);error="Could not query swapchain images";return false;
        }
        nativeScene_.destroy(); // Framebuffers/image views must die before their swapchain.
        if(swapchain_)vkDestroySwapchainKHR(device_,swapchain_,nullptr);
        swapchain_=newSwapchain;images_=std::move(newImages);
        for(auto semaphore:finished_)if(semaphore)vkDestroySemaphore(device_,semaphore,nullptr);
        finished_.assign(images_.size(),VK_NULL_HANDLE);
        VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        for(auto& semaphore:finished_){
            if(vkCreateSemaphore(device_,&semInfo,nullptr,&semaphore)!=VK_SUCCESS){
                error="Could not create per-image presentation semaphore";return false;
            }
        }
        previouslyPresented_.assign(images_.size(),false);
        format_=chosen.format;extent_=newExtent;
        ++telemetry_.resizeCount;
        if(!createStaging(error))return false;
        std::string nativeError;
        const bool nativeReady=nativeRenderTarget_ && nativeScene_.initialize(
            gpu_,device_,format_,extent_,images_,limits_.streamingBudgetBytes,nativeError);
        {
            // A successful Vulkan swapchain is not proof of native 3D.
            // Log native pipeline readiness separately so users can distinguish
            // GPU triangle rendering from CPU framebuffer presentation.
            VkPhysicalDeviceProperties deviceProperties{};
            vkGetPhysicalDeviceProperties(gpu_,&deviceProperties);
            std::ofstream log("graphics_backend.log",std::ios::app);
            if(log){
                log<<"Vulkan device: "<<deviceProperties.deviceName
                   <<", Vulkan API "<<VK_VERSION_MAJOR(deviceProperties.apiVersion)<<'.'
                   <<VK_VERSION_MINOR(deviceProperties.apiVersion)<<'.'
                   <<VK_VERSION_PATCH(deviceProperties.apiVersion)<<'\n';
                log<<"Vulkan WSI "<<extent_.width<<'x'<<extent_.height
                   <<", native 3D: "<<(nativeReady?"READY":"UNAVAILABLE")<<'\n';
                if(!nativeReady)log<<"Native 3D reason: "
                    <<(nativeRenderTarget_?nativeError:"Swapchain color attachment unsupported")<<'\n';
            }
        }
        return true;
    }
public:
    ~VulkanBackend() override{destroy();}
    GraphicsAPI api()const noexcept override{return GraphicsAPI::Vulkan;}
    bool supportsNativeScene()const noexcept override{return initialized_&&nativeScene_.ready();}
    bool initialize(const NativeRenderWindow& native,const GraphicsLimits& limits,std::string& error)override{
        native_=native;limits_=limits;
        std::string graphError;
        if(!RenderGraphPlan::cpuFrameUpload().validate(graphError)){
            error="Invalid Vulkan transfer graph: "+graphError;return false;
        }
        const char* surfaceExtension=nullptr;
#ifdef _WIN32
        if(native.system!=WindowSystem::Win32){error="Win32 Vulkan build requires an HWND";return false;}
        surfaceExtension=VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
#elif defined(__linux__)
        if(native.system!=WindowSystem::Xlib||!native.display){error="Linux Vulkan build requires Xlib Display*";return false;}
        surfaceExtension=VK_KHR_XLIB_SURFACE_EXTENSION_NAME;
#else
        error="Unsupported Vulkan window platform";return false;
#endif
        if(!hasInstanceExtension(VK_KHR_SURFACE_EXTENSION_NAME)||!hasInstanceExtension(surfaceExtension)){
            error="Vulkan surface extensions unavailable";return false;
        }
        const char* validationSetting=std::getenv("GZ_VULKAN_VALIDATION");
        const bool validation=validationSetting && std::strcmp(validationSetting,"1")==0;
        const char* validationLayer="VK_LAYER_KHRONOS_validation";
        const char* extensions[]={VK_KHR_SURFACE_EXTENSION_NAME,surfaceExtension,
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME,VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME};
        VkDebugUtilsMessengerCreateInfoEXT debugInfo{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        debugInfo.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugInfo.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugInfo.pfnUserCallback=validationMessage;debugInfo.pUserData=this;
        const VkValidationFeatureEnableEXT synchronization=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;
        VkValidationFeaturesEXT features{VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};
        features.enabledValidationFeatureCount=1;features.pEnabledValidationFeatures=&synchronization;
        features.pNext=&debugInfo;
        VkApplicationInfo info{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        info.pApplicationName="GRAY ZONE: 1989";info.applicationVersion=VK_MAKE_VERSION(0,0,21);
        info.pEngineName="GZ89";info.engineVersion=VK_MAKE_VERSION(0,0,21);info.apiVersion=VK_API_VERSION_1_0;
        VkInstanceCreateInfo ic{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        ic.pApplicationInfo=&info;ic.enabledExtensionCount=validation?4:2;ic.ppEnabledExtensionNames=extensions;
        if(validation){ic.enabledLayerCount=1;ic.ppEnabledLayerNames=&validationLayer;ic.pNext=&features;}
        if(vkCreateInstance(&ic,nullptr,&instance_)!=VK_SUCCESS){error="vkCreateInstance failed";return false;}
        if(validation){
            const auto createDebug=reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance_,"vkCreateDebugUtilsMessengerEXT"));
            if(!createDebug || createDebug(instance_,&debugInfo,nullptr,&debugMessenger_)!=VK_SUCCESS){
                error="Could not enable Vulkan validation diagnostics";return false;
            }
        }
#ifdef _WIN32
        VkWin32SurfaceCreateInfoKHR si{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
        si.hinstance=GetModuleHandleW(nullptr);si.hwnd=reinterpret_cast<HWND>(native.window);
        if(vkCreateWin32SurfaceKHR(instance_,&si,nullptr,&surface_)!=VK_SUCCESS){error="vkCreateWin32SurfaceKHR failed";return false;}
#elif defined(__linux__)
        VkXlibSurfaceCreateInfoKHR si{VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR};
        si.dpy=reinterpret_cast<Display*>(native.display);si.window=static_cast<Window>(native.window);
        if(vkCreateXlibSurfaceKHR(instance_,&si,nullptr,&surface_)!=VK_SUCCESS){error="vkCreateXlibSurfaceKHR failed";return false;}
#endif
        std::uint32_t count=0;
        if(vkEnumeratePhysicalDevices(instance_,&count,nullptr)!=VK_SUCCESS||!count){error="No Vulkan-capable GPU";return false;}
        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(instance_,&count,devices.data());
        for(const auto candidate:devices){
            if(!hasDeviceExtension(candidate,VK_KHR_SWAPCHAIN_EXTENSION_NAME))continue;
            std::uint32_t families=0;vkGetPhysicalDeviceQueueFamilyProperties(candidate,&families,nullptr);
            std::vector<VkQueueFamilyProperties> properties(families);
            vkGetPhysicalDeviceQueueFamilyProperties(candidate,&families,properties.data());
            for(std::uint32_t i=0;i<families;++i){
                VkBool32 supported=VK_FALSE;
                if((properties[i].queueFlags&VK_QUEUE_GRAPHICS_BIT) &&
                   vkGetPhysicalDeviceSurfaceSupportKHR(candidate,i,surface_,&supported)==VK_SUCCESS && supported){
                    gpu_=candidate;queueFamily_=i;break;
                }
            }
            if(gpu_)break;
        }
        if(!gpu_){error="No Vulkan queue with graphics+presentation support";return false;}
        const float priority=1.f;
        VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        qi.queueFamilyIndex=queueFamily_;qi.queueCount=1;qi.pQueuePriorities=&priority;
        const char* deviceExtensions[]={VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        di.queueCreateInfoCount=1;di.pQueueCreateInfos=&qi;
        di.enabledExtensionCount=1;di.ppEnabledExtensionNames=deviceExtensions;
        if(vkCreateDevice(gpu_,&di,nullptr,&device_)!=VK_SUCCESS){error="vkCreateDevice failed";return false;}
        vkGetDeviceQueue(device_,queueFamily_,0,&queue_);
        VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        pi.queueFamilyIndex=queueFamily_;pi.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        if(vkCreateCommandPool(device_,&pi,nullptr,&pool_)!=VK_SUCCESS){error="vkCreateCommandPool failed";return false;}
        VkCommandBufferAllocateInfo ci{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ci.commandPool=pool_;ci.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ci.commandBufferCount=1;
        if(vkAllocateCommandBuffers(device_,&ci,&command_)!=VK_SUCCESS){error="vkAllocateCommandBuffers failed";return false;}
        VkSemaphoreCreateInfo sem{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        if(vkCreateSemaphore(device_,&sem,nullptr,&acquired_)!=VK_SUCCESS){error="Vulkan semaphore creation failed";return false;}
        VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};fi.flags=VK_FENCE_CREATE_SIGNALED_BIT;
        if(vkCreateFence(device_,&fi,nullptr,&fence_)!=VK_SUCCESS){error="vkCreateFence failed";return false;}
        if(!createSwapchain(native.width,native.height,error))return false;
        initialized_=true;
        return true;
    }
    bool resize(std::uint32_t w,std::uint32_t h,std::string& error)override{
        if(!device_||!w||!h){error="Cannot resize uninitialized/minimized Vulkan surface";return false;}
        if(vkDeviceWaitIdle(device_)!=VK_SUCCESS){error="vkDeviceWaitIdle failed on resize";return false;}
        return createSwapchain(w,h,error);
    }
    bool submit(const RenderSubmission& submission,std::string& error)override{
        const FramePixels& frame=submission.fallbackFrame;
        const bool requestedNative=submission.camera.enabled&&submission.worldMesh.valid()&&nativeScene_.ready();
        if(!initialized_||(!frame.valid()&&!requestedNative)){error="Invalid Vulkan frame";return false;}
        if(vkWaitForFences(device_,1,&fence_,VK_TRUE,UINT64_MAX)!=VK_SUCCESS){error="GPU fence wait failed";return false;}
        std::uint32_t image=0;
        VkResult result=vkAcquireNextImageKHR(device_,swapchain_,UINT64_MAX,acquired_,VK_NULL_HANDLE,&image);
        if(result==VK_ERROR_OUT_OF_DATE_KHR){error="Vulkan swapchain outdated (resize required)";return false;}
        if(result!=VK_SUCCESS && result!=VK_SUBOPTIMAL_KHR){error="vkAcquireNextImageKHR failed";return false;}
        if(image>=images_.size()){error="Vulkan swapchain image array invalid";return false;}
        bool native=requestedNative;
        std::uint64_t meshBytes=0;
        if(native && !nativeScene_.upload(submission.worldMesh,meshBytes,error)){
            // Do not present a stale CPU image: platform re-renders in software.
            return false;
        }
        if(!native){
            if(!mapped_||!frame.valid()){error="Vulkan fallback framebuffer missing";return false;}
            auto* target=static_cast<std::uint32_t*>(mapped_);
            const bool bgra=(format_==VK_FORMAT_B8G8R8A8_UNORM || format_==VK_FORMAT_B8G8R8A8_SRGB);
            for(std::uint32_t y=0;y<extent_.height;++y){
                const auto* source=frame.data+std::size_t(y)*frame.height/extent_.height*frame.stridePixels;
                for(std::uint32_t x=0;x<extent_.width;++x){
                    const auto pixel=source[std::size_t(x)*frame.width/extent_.width];
                    target[std::size_t(y)*extent_.width+x]=bgra?(pixel|0xff000000u):
                        (0xff000000u|(pixel&0x0000ff00u)|((pixel&0x00ff0000u)>>16)|((pixel&0x000000ffu)<<16));
                }
            }
            if(!coherent_){
                VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
                range.memory=memory_;range.offset=0;range.size=VK_WHOLE_SIZE;
                if(vkFlushMappedMemoryRanges(device_,1,&range)!=VK_SUCCESS){error="vkFlushMappedMemoryRanges failed";return false;}
            }
        }
        if(vkResetCommandBuffer(command_,0)!=VK_SUCCESS){error="vkResetCommandBuffer failed";return false;}
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if(vkBeginCommandBuffer(command_,&begin)!=VK_SUCCESS){error="vkBeginCommandBuffer failed";return false;}
        if(native){
            const auto constants=makeGpuFrameConstants(submission.camera,extent_.width,extent_.height,
                                                        submission.visualTimeSeconds);
            nativeScene_.record(command_,image,constants);
        }else{
            VkImageMemoryBarrier toTransfer{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            toTransfer.oldLayout=previouslyPresented_[image]?VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:VK_IMAGE_LAYOUT_UNDEFINED;
            toTransfer.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toTransfer.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
            toTransfer.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
            toTransfer.image=images_[image];
            toTransfer.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
            toTransfer.subresourceRange.levelCount=1;toTransfer.subresourceRange.layerCount=1;
            toTransfer.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(command_,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,
                                 0,nullptr,0,nullptr,1,&toTransfer);
            VkBufferImageCopy region{};region.bufferOffset=0;
            region.imageSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.layerCount=1;
            region.imageExtent={extent_.width,extent_.height,1};
            vkCmdCopyBufferToImage(command_,staging_,images_[image],VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region);
            VkImageMemoryBarrier toPresent{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            toPresent.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
            toPresent.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toPresent.newLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            toPresent.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
            toPresent.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
            toPresent.image=images_[image];
            toPresent.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
            toPresent.subresourceRange.levelCount=1;toPresent.subresourceRange.layerCount=1;
            vkCmdPipelineBarrier(command_,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,0,
                                 0,nullptr,0,nullptr,1,&toPresent);
        }
        if(vkEndCommandBuffer(command_)!=VK_SUCCESS){error="vkEndCommandBuffer failed";return false;}
        if(vkResetFences(device_,1,&fence_)!=VK_SUCCESS){error="vkResetFences failed";return false;}
        VkPipelineStageFlags waitStage=native?VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT:VK_PIPELINE_STAGE_TRANSFER_BIT;
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.waitSemaphoreCount=1;submit.pWaitSemaphores=&acquired_;submit.pWaitDstStageMask=&waitStage;
        submit.commandBufferCount=1;submit.pCommandBuffers=&command_;
        submit.signalSemaphoreCount=1;submit.pSignalSemaphores=&finished_[image];
        if(vkQueueSubmit(queue_,1,&submit,fence_)!=VK_SUCCESS){error="vkQueueSubmit failed";return false;}
        VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        presentInfo.waitSemaphoreCount=1;presentInfo.pWaitSemaphores=&finished_[image];
        presentInfo.swapchainCount=1;presentInfo.pSwapchains=&swapchain_;presentInfo.pImageIndices=&image;
        result=vkQueuePresentKHR(queue_,&presentInfo);
        if(result!=VK_SUCCESS && result!=VK_SUBOPTIMAL_KHR){error="vkQueuePresentKHR failed";return false;}
        previouslyPresented_[image]=true;
        ++telemetry_.completedFrames;
        if(native){
            ++telemetry_.nativeSceneFrames;
            if(telemetry_.nativeSceneFrames==1){
                std::ofstream log("graphics_backend.log",std::ios::app);
                if(log)log<<"FIRST NATIVE GPU FRAME PRESENTED: "
                          <<nativeScene_.triangleCount()<<" indexed triangles, "
                          <<meshBytes<<" mesh bytes.\n";
            }
            telemetry_.nativeSceneTriangles+=nativeScene_.triangleCount();
            telemetry_.meshUploadedBytes+=meshBytes;
            if(!meshBytes)++telemetry_.skippedMeshUploads;
            telemetry_.meshResidentBytes=nativeScene_.residentBytes();
            telemetry_.uploadedBytes+=meshBytes;
        }else{
            telemetry_.uploadedBytes+=std::uint64_t(extent_.width)*extent_.height*4;
        }
        return true;
    }
    void waitIdle()noexcept override{if(device_)vkDeviceWaitIdle(device_);}
    bool ready()const noexcept override{return initialized_;}
    const RenderTelemetry& telemetry()const noexcept override{
        telemetry_.validationErrors=validationErrors_.load(std::memory_order_relaxed);return telemetry_;
    }
};
} // namespace
std::unique_ptr<IRenderBackend> makeVulkanBackend(){return std::make_unique<VulkanBackend>();}
} // namespace gz
