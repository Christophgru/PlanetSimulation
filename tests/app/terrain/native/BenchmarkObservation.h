#pragma once

// Private native benchmark receipts. Inspect owned CPU scalars only; never read
// GPU buffers or query driver memory while a production frame is running.
template<class State>
inline nlohmann::json benchmarkObservation(State& r, GLFWwindow* window) {
    int width=0,height=0;glfwGetFramebufferSize(window,&width,&height);
    nlohmann::json bodies=nlohmann::json::array();
    for(std::size_t i=0;i<r.meshes.planetMeshes.size();++i) {
        const auto& mesh=r.meshes.planetMeshes[i];const auto& t=mesh.terrainStats;
        const auto grass=r.grass.procedural.stats(i);
        const auto& configured=r.scene.scenario.planets[i].foliage;
        const auto eye=r.grass.procedural.planningEye(i);
        bodies.push_back({{"body",i},{"triangles",r.meshTriangles[i]},
            {"unique_samples",t.uniqueSamples},{"topology_input_bytes",t.topologyInputBytes},
            {"gpu_input_bytes",t.gpuInputBytes},{"planning_evaluations",t.planningQueries.evaluations},
            {"bulk_evaluations",t.evaluationQueries.evaluations},
            {"cpu_render_bytes",mesh.vertices.size()*sizeof(float)+mesh.indices.size()*sizeof(unsigned)},
            {"terrain_plan_eye",{r.lastTerrainEyes[i].x,r.lastTerrainEyes[i].y,r.lastTerrainEyes[i].z}},
            {"foliage",{{"configured_density",configured.density_per_m2},
                {"configured_budget",configured.max_blades},{"configured_distance_m",configured.draw_distance_m},
                {"density",grass.density},{"distance_m",grass.distanceMeters},
                {"enabled",configured.enabled},
                {"policy",r.grass.procedural.policy(i)},
                {"budget",configured.enabled ? (grass.allocationBudget ? grass.allocationBudget : std::uint64_t(configured.max_blades)) : 0},
                {"budget_source",grass.allocationBudget ? (r.terrainPublication ? "resident_summary" : "cpu_policy") : "legacy_configured_hard_cap"},
                {"candidates",grass.candidates},{"patches",grass.patches},
                {"gpu_bytes",grass.gpuBytes},{"compute_placement",r.grass.procedural.usesCompute(i)},
                {"plan_eye",eye ? nlohmann::json{eye->x,eye->y,eye->z} : nlohmann::json(nullptr)}}}});
    }
    return {{"viewport",{width,height}},{"scene_size",{r.lastQualitySize.first,r.lastQualitySize.second}},
        {"quality_scale",r.adaptiveQuality.scale()},{"swap_interval_requested",0},{"bodies",std::move(bodies)}};
}
