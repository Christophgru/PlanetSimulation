#pragma once
#include "rendering/diagnostics/memory/MemorySampler.h"
namespace rendering {
MemoryContext contextMemory(); // Context setup only; never on the sampler thread.
NvxMemory contextNvxMemory(); // Setup/shutdown checkpoints only.
}
