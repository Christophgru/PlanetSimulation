#include "rendering/runtime/RendererState.h"
#include "rendering/foliage/GrassPlacement.h"
#include "rendering/geometry/contacts/SparseTerrainContacts.h"

namespace rendering {
void Renderer::Impl::publishResidentBuilds(std::vector<std::optional<TerrainCpuBuild>> builds,
    const std::vector<std::optional<TerrainBuildIdentity>>& identities,
    const glm::dvec3& eye,std::optional<double> characterElapsed) {
    CpuTrace::Scope scope("terrain.complete_capture_publication");
    if(!options.renderTestMode) throw std::logic_error("Interactive compute publication remains gated");
    terrainPublication->pollRetired();
    auto grassEyeWorld=eye;
    plannedCharacterEye.reset();
    std::optional<glm::dvec3> replayGrassEye;
    const auto selected=scene.scenario.surface_camera.planet_index;
    if(characterElapsed) {
        auto contacts=builds[selected] ? builds[selected]->contacts : meshes.planetMeshes[selected].contacts;
        const auto revision=meshes.planetMeshes[selected].revision+(builds[selected] ? 1 : 0);
        grassEyeWorld=previewAstronautEye(*characterElapsed,std::move(contacts),revision);
        plannedCharacterEye=grassEyeWorld;
        if(!astronautReplayRestored && !options.replayPath.empty()) {
            const auto& replay=source.replayDocument;
            if(replay.contains("astronaut_pose") && replay.at("astronaut_pose").contains("grass_plan_eye")) {
                const auto& j=replay.at("astronaut_pose").at("grass_plan_eye");
                if(!j.is_array() || j.size()!=3) throw std::invalid_argument("Grass replay needs three-vector planning eye");
                replayGrassEye=glm::dvec3(j.at(0).get<double>(),j.at(1).get<double>(),j.at(2).get<double>());
            }
        }
    }
    for(std::size_t i=0;i<builds.size();++i) {
        const auto& planet=scene.scenario.planets[i];
        const auto planEye=i==selected && replayGrassEye ? *replayGrassEye : scene.bodies[i+1].toLocalPoint(grassEyeWorld)/planet.radius;
        const auto oldEye=grass.procedural.planningEye(i);
        const bool grassChanged=!builds[i] && (bool(i==selected && replayGrassEye) ||
            (planet.foliage.enabled && (!oldEye || glm::length(planEye-*oldEye)*planet.radius*scene.scenario.metersPerWorldUnit()>=grassRebuildDistance(planet.foliage))));
        if(!builds[i] && !grassChanged) continue;
        terrainPublication->waitRetiredForCapture(i);
        std::vector<int> zones;
        TerrainBuildIdentity current;
        if(builds[i]) {
            zones=builds[i]->geometry.faceZones;current=*identities[i];
            profiler.terrainBuild(builds[i]->milliseconds);
            if(!terrainPublication->submit(std::move(*builds[i]),current,planet,scene.scenario.metersPerWorldUnit(),planEye,
                meshes.planetMeshes[i],meshes.waterMeshes[i],*terrainCompute))
                throw std::logic_error("Capture terrain publication slot is busy");
        } else {
            current=terrainPublication->installed(i).identity;
            if(!terrainPublication->submitGrass(i,planet,scene.scenario.metersPerWorldUnit(),planEye,
                meshes.planetMeshes[i],meshes.waterMeshes[i]))
                throw std::logic_error("Capture grass publication slot is busy");
        }
        terrainPublication->waitForCapture();
        if(!terrainPublication->publish(current,meshes.planetMeshes[i],meshes.waterMeshes[i]))
            throw std::logic_error("Capture terrain publication became obsolete");
        // No allocating consumer updates after the complete ownership transfer.
        const auto& receipt=terrainPublication->installed(i);
        if(builds[i]) {
            lastFaceZones[i].swap(zones);meshZoneFaces[i]=receipt.zoneFaces;meshTriangles[i]=receipt.triangles;
            meshSteepRefinedFaces[i]=receipt.steepRefinedFaces;lastTerrainEyes[i]=receipt.identity.eye;
            lastLocalMask[i]=receipt.identity.localMask;meshReady[i]=true;
            installedTerrainSerial[i]=receipt.identity.serial;terrainFailures[i].reset();
            terrainShadows.invalidate(i);profiler.meshUpload();
            if(i==selected) astronautGround.bind(meshes.planetMeshes[i].contacts,receipt.landRevision);
        }
        frameReuse.invalidate();
        profiler.foliagePreparation(1,0,0,0,grass.procedural.stats(i).metadataInputBytes+grass.procedural.stats(i).allocationInputBytes);
    }
}
nlohmann::json Renderer::Impl::terrainPublicationState() const {
    nlohmann::json result={{"managed",bool(terrainPublication)}};
    if(!terrainPublication) return result;
    const auto& s=terrainPublication->stats();
    result.update({{"published",s.published},{"grass_only_published",s.grassOnlyPublished},
        {"failed",s.failed},{"obsolete",s.obsolete},{"retired",s.retired},
        {"pending",terrainPublication->pending()},{"reserved_bytes",terrainPublication->reservedBytes()},
        {"peak_reserved_bytes",s.peakReservedBytes},{"character_previews",characterPreviews},
        {"astronaut_contact_revision",astronautGround.revision()},{"consumers",nlohmann::json::array()}});
    const auto key=[](const TerrainGenerationKey& k) {
        return nlohmann::json{{"field",std::to_string(k.field)},{"topology",std::to_string(k.topology)},
            {"field_version",k.fieldVersion},{"topology_version",k.topologyVersion}};
    };
    for(std::size_t i=0;i<terrainConsumers.size();++i) {
        const auto& c=terrainConsumers[i];const auto& receipt=terrainPublication->installed(i);
        result["consumers"].push_back({{"body",receipt.identity.bodyName},{"epoch",receipt.identity.epoch},
            {"serial",receipt.identity.serial},{"land",key(c.land)},{"water",key(c.water)},
            {"grass",key(c.grass)},{"contacts",key(c.contacts)},
            {"land_revision",c.landRevision},{"water_revision",c.waterRevision},{"grass_revision",c.grassRevision},
            {"main_revision",c.mainRevision},{"reflection_revision",c.reflectionRevision},{"shadow_revision",c.shadowRevision},
            {"water_draw_revision",c.waterDrawRevision},{"grass_draw_revision",c.grassDrawRevision},
            {"land_buffer",meshes.planetMeshes[i].vbo},{"water_buffer",meshes.waterMeshes[i].vbo},
            {"grass_plan_eye",{receipt.grassEye.x,receipt.grassEye.y,receipt.grassEye.z}},
            {"water_enabled",receipt.waterEnabled},{"shadows_enabled",scene.scenario.lighting.shadows.enabled}});
    }
    return result;
}
}
