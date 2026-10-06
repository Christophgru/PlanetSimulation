#include "rendering/diagnostics/memory/ContextMemory.h"
#include <GL/glew.h>
#include <algorithm>
#include <array>
#include <iomanip>
#include <sstream>
namespace rendering {
NvxMemory contextNvxMemory() {
    NvxMemory result;result.observedNs=memoryClockNs();
    if(!GLEW_NVX_gpu_memory_info) return result;
    GLint dedicated=0,total=0,available=0;
    glGetIntegerv(GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX,&dedicated);
    glGetIntegerv(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX,&total);
    glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX,&available);
    if(dedicated<=0||total<=0||available<0||available>total) {result.status="invalid_counters";return result;}
    result.status="ok";result.dedicated=static_cast<std::uint64_t>(dedicated)*1024;
    result.totalAvailable=static_cast<std::uint64_t>(total)*1024;
    result.available=static_cast<std::uint64_t>(available)*1024;return result;
}
MemoryContext contextMemory() {
    MemoryContext result;
    if(auto vendor=glGetString(GL_VENDOR)) result.vendor=reinterpret_cast<const char*>(vendor);
    if(auto renderer=glGetString(GL_RENDERER)) result.renderer=reinterpret_cast<const char*>(renderer);
    result.nvx=contextNvxMemory();
    if(!(GLEW_EXT_memory_object||GLEW_EXT_semaphore)||!glGetUnsignedBytei_vEXT) return result;
    GLint count=0;glGetIntegerv(GL_NUM_DEVICE_UUIDS_EXT,&count);
    if(count!=1) {result.status="uuid_ambiguous";return result;}
    std::array<GLubyte,16> bytes{};glGetUnsignedBytei_vEXT(GL_DEVICE_UUID_EXT,0,bytes.data());
    if(std::all_of(bytes.begin(),bytes.end(),[](auto b){return b==0;})) {result.status="uuid_invalid";return result;}
    std::ostringstream uuid;uuid<<"GPU-"<<std::hex<<std::setfill('0');
    for(std::size_t i=0;i<bytes.size();++i) {
        if(i==4||i==6||i==8||i==10) uuid<<'-';uuid<<std::setw(2)<<static_cast<unsigned>(bytes[i]);
    }
    result.uuid=uuid.str();result.status="uuid_verified";return result;
}
}
