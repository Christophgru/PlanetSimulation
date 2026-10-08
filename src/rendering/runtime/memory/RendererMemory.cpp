#include "rendering/runtime/RendererState.h"
#include "rendering/runtime/terrain/reload/interactive/PendingReload.h"
#include <algorithm>
namespace rendering {
namespace {
std::uint64_t meshBytes(const Mesh& m) {
    return m.vertices.capacity()*sizeof(float)+m.indices.capacity()*sizeof(unsigned)+m.terrainStats.faceZones.capacity()*sizeof(int);
}
std::uint64_t meshBytes(const std::vector<Mesh>& meshes) {
    std::uint64_t result=0;for(const auto& m:meshes) result+=meshBytes(m);return result;
}
std::uint64_t replacementBytes(const SceneTerrainReplacement& r) {
    std::uint64_t result=0;for(std::size_t i=0;i<r.scene().scenario.planets.size();++i) result+=meshBytes(r.land(i))+meshBytes(r.water(i));
    return result;
}
}
MemoryObservation Renderer::Impl::memorySnapshot(MemoryPhase phase,const SceneTerrainReplacement* replacement,
    const LegacySceneReplacement* legacy,std::uint64_t attempt,std::uint64_t targetEpoch) const {
    MemoryObservation o;o.phase=phase;o.observedNs=memoryClockNs();
    const auto next=profiler.nextFrameNumber();o.frame=next?next-1:0;
    o.reloadAttempt=attempt?attempt:(pendingResidentReload?pendingResidentReload->traceAttempt:0);
    o.replacementEpoch=targetEpoch?targetEpoch:(pendingResidentReload?pendingResidentReload->epoch:0);
    o.epoch=terrainSceneEpoch;o.serial=terrainRequestSerial;o.nvx=memoryNvx;
    o.managedLedger=bool(terrainPublication)||grass.procedural.adaptiveBudget();
    if(terrainPublication) o.liveReserved=terrainPublication->reservedBytes();
    else if(grass.procedural.adaptiveBudget()) o.liveReserved=meshes.terrainBytes()+grass.procedural.ownedBytes();
    if(!replacement && pendingResidentReload) replacement=pendingResidentReload->transaction.get();
    if(replacement) o.replacementReserved=replacement->reservedBytes();
    o.overlapReserved=std::max(o.liveReserved,o.replacementReserved);
    if(!terrainPublication && grass.procedural.adaptiveBudget()) {
        if(legacy) o.replacementReserved=legacy->meshes.terrainBytes()+legacy->grass.procedural.ownedBytes();
        o.overlapReserved=o.liveReserved+o.replacementReserved;
        if(retiredLegacyScene) o.overlapReserved+=retiredLegacyScene->meshes.terrainBytes()+retiredLegacyScene->grass.procedural.ownedBytes();
    }
    o.meshVectorBytes=meshBytes(meshes.planetMeshes)+meshBytes(meshes.waterMeshes);
    if(replacement) o.meshVectorBytes+=replacementBytes(*replacement);
    if(retiredResidentScene) o.meshVectorBytes+=replacementBytes(*retiredResidentScene);
    if(legacy) o.meshVectorBytes+=meshBytes(legacy->meshes.planetMeshes)+meshBytes(legacy->meshes.waterMeshes);
    if(retiredLegacyScene) o.meshVectorBytes+=meshBytes(retiredLegacyScene->meshes.planetMeshes)+meshBytes(retiredLegacyScene->meshes.waterMeshes);
    if(pendingResidentReload) o.meshVectorBytes+=meshBytes(pendingResidentReload->previewLand);
    if(const auto s=terrainJobs.memorySnapshot()) {
        o.schedulerObserved=true;o.readyVectorBytes=s->readyVectorBytes;
        o.running=s->running;o.queued=s->queued;o.ready=s->ready;
    }
    return o;
}
void Renderer::Impl::observeMemory(MemoryPhase phase,bool event,const SceneTerrainReplacement* replacement,
    const LegacySceneReplacement* legacy,std::uint64_t attempt,std::uint64_t targetEpoch) {
    if(grass.procedural.adaptiveBudget()) {
        if(!terrainPublication) {
            grass.procedural.terrainBudgetBytes(meshes.terrainBytes());
            grass.procedural.externalBudgetBytes(retiredLegacyScene ?
                retiredLegacyScene->meshes.terrainBytes()+retiredLegacyScene->grass.procedural.ownedBytes() : 0);
        }
        auto signals=grass.procedural.budgetSignals();signals.nowNs=memoryClockNs();signals.capBytes=options.videoMemoryCapBytes;
        if(memorySampler) if(const auto cached=memorySampler->cached()) {
            signals.memoryNs=cached->sampledNs;signals.sampleReserved=cached->reserved;
            signals.freeBytes=cached->physical.status=="ok" ? std::optional<std::uint64_t>(cached->physical.free) : std::nullopt;
        }
        if(profiler.gpuReady()) {
            signals.gpuMs=profiler.gpuMilliseconds;signals.gpuSample=profiler.gpuSample();signals.gpuNs=profiler.gpuSampleNs();
        }
        signals.utilization=gpuUtilization.sample(true);
        grass.procedural.observeBudget(std::move(signals));
    }
    if(memorySampler) {
        const bool transition=phase!=memoryPhase;memoryPhase=phase;
        memorySampler->observe(memorySnapshot(phase,replacement,legacy,attempt,targetEpoch),event||transition);
    }
}
}
