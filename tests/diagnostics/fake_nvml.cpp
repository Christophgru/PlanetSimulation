// Driver-free ABI fixture. Only the telemetry tests load this library.
#include <cstdio>
#include <cstdlib>
static int setting(const char* name, int fallback) {
    const char* value = std::getenv(name);
    return value ? std::atoi(value) : fallback;
}
extern "C" {
int nvmlInit_v2() { return setting("TEST_NVML_INIT_ERROR", 0); }
int nvmlShutdown() { return 0; }
int nvmlDeviceGetCount_v2(unsigned* count) { *count = setting("TEST_NVML_COUNT", 1); return 0; }
int nvmlDeviceGetHandleByIndex_v2(unsigned, void** device) { *device = reinterpret_cast<void*>(1); return 0; }
int nvmlDeviceGetName(void*, char* name, unsigned length) {
    std::snprintf(name, length, "NVIDIA Quadro M1000M"); return 0;
}
struct Rates { unsigned gpu, memory; };
int nvmlDeviceGetUtilizationRates(void*, Rates* rates) {
    rates->gpu = setting("TEST_NVML_GPU", 42); rates->memory = 7;
    return setting("TEST_NVML_ERROR", 0);
}
}
