#include <cstdio>
#include <cstdlib>
static int setting(const char* key,int value=0) {const auto* text=std::getenv(key);return text?std::atoi(text):value;}
extern "C" {
int nvmlInit_v2(){return setting("MEMORY_INIT_ERROR");}
int nvmlShutdown(){return 0;}
int nvmlDeviceGetHandleByUUID(const char*,void** device){*device=reinterpret_cast<void*>(1);return setting("MEMORY_LOOKUP_ERROR");}
int nvmlDeviceGetUUID(void*,char* uuid,unsigned length){std::snprintf(uuid,length,"%s",setting("MEMORY_WRONG_UUID")?"GPU-22222222-2222-2222-2222-222222222222":"GPU-11111111-1111-1111-1111-111111111111");return setting("MEMORY_UUID_ERROR");}
struct Bytes {unsigned long long total,free,used;};
int nvmlDeviceGetMemoryInfo(void*,Bytes* bytes){*bytes={1024,static_cast<unsigned long long>(setting("MEMORY_FREE",256)),static_cast<unsigned long long>(setting("MEMORY_USED",768))};return setting("MEMORY_READ_ERROR");}
}
