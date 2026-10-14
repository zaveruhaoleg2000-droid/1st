// Standalone Vulkan runtime/driver probe. Builds without Vulkan SDK.
// Does not depend on a window or a Vulkan header: useful on CPU-only installations.
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#ifdef _WIN32
#  define NOMINMAX
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif
namespace {
#ifdef _WIN32
using LibraryHandle=HMODULE;
LibraryHandle openLibrary(){return LoadLibraryA("vulkan-1.dll");}
void* loadSymbol(LibraryHandle h,const char* name){return reinterpret_cast<void*>(GetProcAddress(h,name));}
void closeLibrary(LibraryHandle h){if(h)FreeLibrary(h);}
#else
using LibraryHandle=void*;
LibraryHandle openLibrary(){return dlopen("libvulkan.so.1",RTLD_NOW|RTLD_LOCAL);}
void* loadSymbol(LibraryHandle h,const char* name){return dlsym(h,name);}
void closeLibrary(LibraryHandle h){if(h)dlclose(h);}
#endif
struct AppInfo {
    std::uint32_t sType=0;const void* pNext=nullptr;
    const char* name=nullptr;std::uint32_t applicationVersion=1;
    const char* engine=nullptr;std::uint32_t engineVersion=1,apiVersion=0x00400000u;
};
struct InstanceInfo {
    std::uint32_t sType=1;const void* pNext=nullptr;std::uint32_t flags=0;
    const AppInfo* application=nullptr;std::uint32_t layerCount=0;
    const char* const* layers=nullptr;std::uint32_t extensionCount=0;
    const char* const* extensions=nullptr;
};
using Handle=void*;
using EnumerateVersion=int (*)(std::uint32_t*);
using CreateInstance=int (*)(const InstanceInfo*,const void*,Handle*);
using EnumerateDevices=int (*)(Handle,std::uint32_t*,Handle*);
using GetPhysicalDeviceProperties=void (*)(Handle,void*);
using DestroyInstance=void (*)(Handle,const void*);
const char* vkError(int r){
    switch(r){case 0:return "VK_SUCCESS";case -1:return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case -2:return "VK_ERROR_OUT_OF_DEVICE_MEMORY";case -3:return "VK_ERROR_INITIALIZATION_FAILED";
        case -7:return "VK_ERROR_EXTENSION_NOT_PRESENT";case -9:return "VK_ERROR_INCOMPATIBLE_DRIVER";
        default:return "VkResult (see numeric code)";}
}
std::string version(std::uint32_t api){
    return std::to_string(api>>22)+"."+std::to_string((api>>12)&0x3ffu)+"."+std::to_string(api&0xfffu);
}
}
int main(int argc,char** argv){
    // -o optional output file; default in working directory.
    const std::string path=argc>=3&&std::string(argv[1])=="-o"?argv[2]:"gpu_probe.txt";
    std::ofstream report(path,std::ios::trunc);
    auto emit=[&](const std::string& line){std::cout<<line<<'\n';if(report)report<<line<<'\n';};
    emit("GRAY ZONE: 1989 - Vulkan driver probe v0.0.22");
    emit("This is a loader/device probe, not a rendered GPU frame.");
    const auto lib=openLibrary();
    if(!lib){emit("Vulkan loader: NOT INSTALLED");return 2;}
    emit("Vulkan loader: FOUND");
    const auto pVersion=reinterpret_cast<EnumerateVersion>(loadSymbol(lib,"vkEnumerateInstanceVersion"));
    if(pVersion){std::uint32_t api=0;const int r=pVersion(&api);
        emit("Vulkan loader API: "+(r==0?version(api):std::string(vkError(r))));}
    else emit("Vulkan loader API: 1.0 (version query not exported)");
    const auto create=reinterpret_cast<CreateInstance>(loadSymbol(lib,"vkCreateInstance"));
    const auto enumerate=reinterpret_cast<EnumerateDevices>(loadSymbol(lib,"vkEnumeratePhysicalDevices"));
    const auto properties=reinterpret_cast<GetPhysicalDeviceProperties>(loadSymbol(lib,"vkGetPhysicalDeviceProperties"));
    const auto destroy=reinterpret_cast<DestroyInstance>(loadSymbol(lib,"vkDestroyInstance"));
    if(!create||!enumerate||!destroy){emit("Required Vulkan functions: MISSING");closeLibrary(lib);return 3;}
    const AppInfo app{0,nullptr,"GRAY ZONE 1989 GPU probe",1,"GZ89",1,0x00400000u};
    const InstanceInfo ci{1,nullptr,0,&app,0,nullptr,0,nullptr};
    Handle instance=nullptr;
    const int r=create(&ci,nullptr,&instance);
    emit(std::string("vkCreateInstance: ")+vkError(r)+" ("+std::to_string(r)+")");
    if(r!=0||!instance){
        if(r==-9)emit("RESULT: Vulkan driver/ICD unavailable or incompatible with Vulkan 1.0.");
        else emit("RESULT: Cannot create Vulkan instance on this system.");
        closeLibrary(lib);return 4;
    }
    std::uint32_t count=0;
    const int rDevices=enumerate(instance,&count,nullptr);
    emit("vkEnumeratePhysicalDevices: "+std::string(vkError(rDevices))+" ("+std::to_string(rDevices)+")");
    emit("Vulkan physical devices: "+std::to_string(count));
    if(rDevices==0 && count && count<=128){
        std::vector<Handle> handles(count);
        if(enumerate(instance,&count,handles.data())==0 && properties){
            for(std::uint32_t i=0;i<count;++i){
                // VkPhysicalDeviceProperties begins with 5 uint32_t values and
                // a fixed 256-byte UTF-8 name. Opaque aligned storage leaves
                // room for its much larger implementation-defined tail.
                alignas(16) unsigned char bytes[4096]{};
                properties(handles[i],bytes);
                std::uint32_t api=0;std::memcpy(&api,bytes,sizeof(api));
                char name[257]{};std::memcpy(name,bytes+20,256);name[256]='\0';
                emit("GPU "+std::to_string(i)+": "+std::string(name)+" (Vulkan "+version(api)+")");
            }
        }
    }
    destroy(instance,nullptr);closeLibrary(lib);
    emit(count&&rDevices==0?"RESULT: A Vulkan driver and physical device are available.":
                         "RESULT: No Vulkan physical device is available.");
    return count&&rDevices==0?0:5;
}
