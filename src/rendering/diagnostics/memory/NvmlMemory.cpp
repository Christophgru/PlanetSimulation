#include "rendering/diagnostics/memory/MemorySampler.h"
#include <algorithm>
#include <array>
#if defined(__linux__)
#include <dlfcn.h>
#endif
namespace rendering {
bool validDeviceUuid(const std::string& uuid) {
    if(uuid.size()!=40 || uuid.substr(0,4)!="GPU-") return false;
    bool nonzero=false;
    for(std::size_t i=4;i<uuid.size();++i) {
        if(i==12||i==17||i==22||i==27) {if(uuid[i]!='-') return false;continue;}
        if(!((uuid[i]>='0'&&uuid[i]<='9')||(uuid[i]>='a'&&uuid[i]<='f'))) return false;
        nonzero|=uuid[i]!='0';
    }
    return nonzero;
}
namespace {
struct Nvml {
#if defined(__linux__)
    void* library=nullptr;
    void* device=nullptr;
    bool initialized=false;
    int (*shutdown)()=nullptr;
    struct Bytes {unsigned long long total,free,used;};
    int (*memory)(void*,Bytes*)=nullptr;
#endif
    PhysicalMemory unavailable;
    explicit Nvml(const MemoryContext& context) {
        unavailable.status="unverified_context";
        if(context.status!="uuid_verified"||!validDeviceUuid(context.uuid)) return;
        unavailable.status="vendor_unsupported";
        if(context.vendor.find("NVIDIA")==std::string::npos) return;
#if defined(__linux__)
        unavailable.status="library_unavailable";
        library=dlopen("libnvidia-ml.so.1",RTLD_NOW|RTLD_LOCAL);
        if(!library) return;
        auto init=reinterpret_cast<int(*)()>(dlsym(library,"nvmlInit_v2"));
        shutdown=reinterpret_cast<int(*)()>(dlsym(library,"nvmlShutdown"));
        auto byUuid=reinterpret_cast<int(*)(const char*,void**)>(dlsym(library,"nvmlDeviceGetHandleByUUID"));
        auto uuid=reinterpret_cast<int(*)(void*,char*,unsigned)>(dlsym(library,"nvmlDeviceGetUUID"));
        memory=reinterpret_cast<int(*)(void*,Bytes*)>(dlsym(library,"nvmlDeviceGetMemoryInfo"));
        unavailable.status="symbols_unavailable";
        if(!init||!shutdown||!byUuid||!uuid||!memory) return;
        unavailable.status="initialization_failed";
        if((unavailable.driverError=init())!=0) return;
        initialized=true;
        unavailable.status="uuid_lookup_failed";
        if((unavailable.driverError=byUuid(context.uuid.c_str(),&device))!=0) return;
        std::array<char,96> identity{};
        unavailable.status="uuid_read_failed";
        if((unavailable.driverError=uuid(device,identity.data(),identity.size()))!=0) return;
        unavailable.status="uuid_mismatch";
        if(!std::char_traits<char>::find(identity.data(),identity.size(),'\0') || context.uuid!=identity.data()) return;
        unavailable.status="ok";
#else
        unavailable.status="platform_unsupported";
#endif
    }
    PhysicalMemory read() {
        if(unavailable.status!="ok") return unavailable;
#if defined(__linux__)
        Bytes bytes{};PhysicalMemory result;
        result.driverError=memory(device,&bytes);
        if(result.driverError) {result.status="read_failed";return result;}
        if(!bytes.total||bytes.used>bytes.total||bytes.free>bytes.total-bytes.used) {
            result.status="invalid_counters";return result;
        }
        result.status="ok";result.total=bytes.total;result.free=bytes.free;result.used=bytes.used;
        return result;
#else
        return unavailable;
#endif
    }
    ~Nvml() {
#if defined(__linux__)
        if(initialized) shutdown();
        if(library) dlclose(library);
#endif
    }
};
}
MemoryReader nvmlMemoryReader(const MemoryContext& context) {
    auto reader=std::make_shared<Nvml>(context);
    return [reader]{return reader->read();};
}
}
