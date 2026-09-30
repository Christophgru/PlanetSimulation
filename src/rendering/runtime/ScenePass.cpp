#include "rendering/runtime/ScenePass.h"
#include "rendering/foliage/GrassWind.h"
#include <glm/gtc/type_ptr.hpp>

namespace rendering {
rendering::CameraExposure renderScene(const config::ScenarioConfig& scenario,
                 const std::vector<simulation::BodyState>& bodies,
                 const glm::mat4& view, float fov,
                 const glm::dvec3& eyeWorld, const Shader& shader,
                 const Shader& waterShader, const Shader& skyboxShader,
                 rendering::WaterReflectionTarget& reflectionTarget,
                 const Shader& shadowShader, rendering::TerrainShadowMaps& shadows,
                 const Shader& atmosphereShader, rendering::AtmosphereRenderer& atmosphere,
                 rendering::AtmosphereRenderer& reflectionAtmosphere,
                 rendering::AtmosphereTransmittance& atmosphereColumns,
                 const Mesh& sunMesh, const Mesh& skyboxMesh,
                 const std::vector<Mesh>& planetMeshes,
                 const std::vector<Mesh>& waterMeshes, int width, int height,
                 rendering::ClipPlanes clip,
                 std::optional<std::size_t> meteredPlanet,
                 bool recordObjects, rendering::FrameProfiler* profiler,
                 bool forceHdr, GLuint outputFramebuffer,
                 rendering::GrassRenderer* grass, double sceneTime) {
    using Stage = rendering::FrameStage;
    using Scope = rendering::FrameProfiler::Scope;
    Scope lightingScope(profiler, Stage::Lighting, false);
    const glm::mat4 projection = rendering::perspectiveProjection(
        fov, static_cast<float>(width) / height, clip);
    const auto& sun = scenario.sun;
    const auto lighting = rendering::calculateLighting(scenario, bodies);
    double skyIlluminance = 0.0;
    if (meteredPlanet && scenario.planets[*meteredPlanet].atmosphere.enabled) {
        const auto& planet = scenario.planets[*meteredPlanet];
        const auto optics = simulation::atmosphereOptics(planet.atmosphere,
            planet.radius * scenario.metersPerWorldUnit(), simulation::referenceAir(planet.atmosphere));
        skyIlluminance = simulation::atmosphericSkyIlluminance(planet.atmosphere, optics,
            (eyeWorld - bodies[*meteredPlanet + 1].position) / planet.radius,
            lighting.planets[*meteredPlanet].sunDirection, lighting.planets[*meteredPlanet].sunlight);
    }
    auto exposure = rendering::cameraExposure(scenario.lighting, lighting, bodies, eyeWorld,
                                                    meteredPlanet, skyIlluminance);
    const bool protectHighlights = scenario.lighting.auto_exposure.enabled && meteredPlanet.has_value();
    const bool hdr = rendering::hasAtmosphere(scenario) || forceHdr;
    exposure.hdrOutput = hdr;
    lightingScope.stop();
    Scope foliageScope(profiler, Stage::Foliage, false);
    if (grass) for (std::size_t i=0;i<scenario.planets.size();++i) {
        const auto& planet=scenario.planets[i];
        const auto prepared = grass->prepare(i,planetMeshes[i],planet,scenario.metersPerWorldUnit(),
            bodies[i+1].toLocalPoint(eyeWorld)/planet.radius);
        if (profiler) profiler->foliagePreparation(prepared.rebuilds,
            prepared.placementMs, prepared.sortMs, prepared.uploadMs);
    }
    foliageScope.stop();
    { Scope tablesScope(profiler, Stage::Tables); atmosphereColumns.ensure(scenario); }
    Scope shadowScope(profiler, Stage::Shadows);
    shadows.ensure(scenario.planets.size(), scenario.lighting.shadows);
    if (scenario.lighting.shadows.enabled) {
        for (std::size_t i = 0; i < scenario.planets.size(); ++i) {
            const auto& planet = scenario.planets[i];
            double heightMeters = planet.terrain_landscape.maximumAbsoluteHeightMeters();
            for (const auto& noise : planet.surface_noise) heightMeters += noise.amplitude_m;
            const double extent = 1.0 + heightMeters / (scenario.metersPerWorldUnit() * planet.radius);
            glm::dvec3 localSun = glm::transpose(bodies[i + 1].orientation) * lighting.planets[i].sunDirection;
            // Coincident centers have no directed sunlight; any map orientation
            // is valid because their diffuse contribution is already zero.
            if (glm::dot(localSun, localSun) == 0.0) localSun = glm::dvec3(0, 0, 1);
            if (shadows.beginIfNeeded(i, shadowShader, localSun, extent, planetMeshes[i].revision)) {
                if (profiler) profiler->shadowUpdate();
                planetMeshes[i].draw();
            } else if (profiler) profiler->shadowReuse();
        }
    }
    shadowScope.stop();
    const auto setRgb = [](const Shader& target, const char* name, const glm::dvec3& value) {
        target.setFloat3(name, static_cast<float>(value.x), static_cast<float>(value.y),
                        static_cast<float>(value.z));
    };
    const auto setBodyLighting = [&](const Shader& target, std::size_t index, float radiusScale = 1.0f) {
        const auto& light = lighting.planets[index];
        setRgb(target, "uSunDirection", light.sunDirection);
        setRgb(target, "uSunlight", light.sunlight);
        setRgb(target, "uIndirectLight", light.reflectedLight + glm::dvec3(scenario.lighting.ambient_light));
        target.setFloat("uExposure", static_cast<float>(exposure.exposure));
        shadows.bindForShading(index, target, scenario.lighting.shadows, radiusScale);
        target.setFloat("uAtmSunAngularRadius", std::asin(std::clamp(scenario.sun.radius /
            std::max(scenario.sun.radius, glm::length(bodies[0].position - bodies[index + 1].position)), 0.0, 1.0)));
        target.setInt("uLinearOutput", hdr);
        target.setFloat("uAtmosphereRadiusScale", radiusScale);
        atmosphereColumns.bind(index, target);
        rendering::bindAtmosphere(target, scenario.planets[index], scenario.metersPerWorldUnit(),
            glm::transpose(bodies[index + 1].orientation) * light.sunDirection);
    };

    auto drawSkybox = [&](const glm::mat4& passProjection,
                          const glm::mat4& passView) {
        if (!scenario.skybox.enabled) return;
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);
        skyboxShader.use();
        skyboxShader.setInt("uLinearOutput", hdr);
        skyboxShader.setFloat("uExposure", exposure.exposure);
        skyboxShader.setFloat("uSkySensitivity", static_cast<float>(exposure.skySensitivity));
        skyboxShader.setMat4("projection", glm::value_ptr(passProjection));
        skyboxShader.setMat4("view", glm::value_ptr(passView));
        skyboxShader.setInt("uSeed", scenario.skybox.seed);
        skyboxShader.setFloat("uStarDensity",
                              static_cast<float>(scenario.skybox.star_density));
        skyboxShader.setFloat("uStarScale",
                              static_cast<float>(scenario.skybox.star_scale));
        skyboxShader.setFloat("uStarBrightness",
                              static_cast<float>(scenario.skybox.star_brightness));
        skyboxShader.setFloat3("uBackgroundColor",
            static_cast<float>(scenario.skybox.background_color[0]),
            static_cast<float>(scenario.skybox.background_color[1]),
            static_cast<float>(scenario.skybox.background_color[2]));
        skyboxShader.setFloat3("uStarColor",
            static_cast<float>(scenario.skybox.star_color[0]),
            static_cast<float>(scenario.skybox.star_color[1]),
            static_cast<float>(scenario.skybox.star_color[2]));
        skyboxMesh.draw();
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
    };

    auto drawOpaqueScene = [&](const glm::mat4& passProjection,
                               const glm::mat4& passView,
                               const glm::vec3& clipCenter,
                               float clipRadius, bool mainPass = false) {
        const bool record = recordObjects && mainPass;
        if (record) {
            glEnable(GL_STENCIL_TEST);
            glStencilMask(0xff);
            glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
            glStencilFunc(GL_ALWAYS, 1, 0xff); // Sun.
        }
        shader.use();
        shader.setInt("uLinearOutput", hdr);
        shader.setMat4("projection", glm::value_ptr(passProjection));
        shader.setMat4("view", glm::value_ptr(passView));
        shader.setFloat3("uClipCenter", clipCenter.x, clipCenter.y, clipCenter.z);
        shader.setFloat("uClipRadius", clipRadius);
        setRgb(shader, "uEmission", lighting.sunEmission);
        shader.setFloat("uExposure", static_cast<float>(exposure.exposure));

        const glm::mat4 sunModel = rendering::sphereModel(
            glm::vec3(bodies[0].position), static_cast<float>(sun.radius));
        shader.setMat4("model", glm::value_ptr(sunModel));
        shader.setFloat3("uColor", static_cast<float>(sun.color[0]),
                         static_cast<float>(sun.color[1]),
                         static_cast<float>(sun.color[2]));
        shader.setFloat("uEmissive", 1.0f);
        shader.setFloat("uTerrainMetersPerRadius", 0.0f);
        sunMesh.draw();

        for (std::size_t i = 0; i < scenario.planets.size(); ++i) {
            if (record) glStencilFunc(GL_ALWAYS, i == meteredPlanet.value_or(0) ? 2 : 3, 0xff);
            const auto& planet = scenario.planets[i];
            shader.use();
            const glm::mat4 model = rendering::sphereModel(
                glm::vec3(bodies[i + 1].position), static_cast<float>(planet.radius),
                glm::mat3(bodies[i + 1].orientation));
            shader.setMat4("model", glm::value_ptr(model));
            shader.setFloat3("uColor", static_cast<float>(planet.color[0]),
                             static_cast<float>(planet.color[1]),
                             static_cast<float>(planet.color[2]));
            shader.setFloat("uEmissive", 0.0f);
            shader.setFloat("uTerrainMetersPerRadius", static_cast<float>(
                planet.radius * scenario.metersPerWorldUnit()));
            const auto rockRange = planet.terrain_material.slopeMetricRange();
            shader.setFloat2("uTerrainRockRange", rockRange[0], rockRange[1]);
            shader.setInt("uLandscapeEnabled",planet.terrain_landscape.enabled);
            double maximumHeight=planet.terrain_landscape.maximumAbsoluteHeightMeters();
            for (const auto& noise:planet.surface_noise) maximumHeight+=noise.amplitude_m;
            shader.setFloat3("uLandscapeLevels",planet.water.enabled ? planet.water.level_m : 0.0,
                             0.1,maximumHeight);
            const glm::dvec3 materialEye = glm::transpose(bodies[i + 1].orientation) *
                (glm::dvec3(glm::inverse(passView)[3]) - bodies[i + 1].position) / planet.radius;
            setRgb(shader, "uTerrainEyeBody", materialEye);
            setBodyLighting(shader, i);
            planetMeshes[i].draw();
            if (grass && grass->count(i)) {
                const auto& settings=planet.foliage;
                auto& bladeShader=grass->shader;
                bladeShader.use();
                bladeShader.setMat4("model",glm::value_ptr(model));
                bladeShader.setMat4("view",glm::value_ptr(passView));
                bladeShader.setMat4("projection",glm::value_ptr(passProjection));
                bladeShader.setFloat3("uClipCenter",clipCenter.x,clipCenter.y,clipCenter.z);
                bladeShader.setFloat("uClipRadius",clipRadius);
                bladeShader.setFloat("uMetersPerRadius",planet.radius*scenario.metersPerWorldUnit());
                bladeShader.setFloat("uGrassHeight",settings.height_m);
                bladeShader.setFloat("uGrassWidth",settings.width_m);
                bladeShader.setFloat("uDrawDistance",settings.draw_distance_m);
                bladeShader.setFloat("uWindStrength",settings.wind_strength);
                bladeShader.setFloat("uTime",rendering::grassWindTime(sceneTime));
                setRgb(bladeShader,"uGrassEyeBody",bodies[i+1].toLocalPoint(eyeWorld)/planet.radius);
                setRgb(bladeShader,"uViewEyeWorld",glm::dvec3(glm::inverse(passView)[3]));
                setBodyLighting(bladeShader,i);
                grass->draw(i);
            }
        }
        if (record) glDisable(GL_STENCIL_TEST);
    };

    Scope opaqueScope(profiler, Stage::Opaque);
    if (hdr) atmosphere.begin(width, height);
    const GLuint sceneFramebuffer = hdr ? atmosphere.framebuffer() : outputFramebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFramebuffer);
    glViewport(0, 0, width, height);
    glm::dvec3 background;
    for (int c = 0; c < 3; ++c) {
        background[c] = scenario.skybox.background_color[c] * exposure.skySensitivity;
        if (hdr) {
            const double srgb = std::clamp(background[c], 0.0, 0.9999);
            const double mapped = srgb <= 0.04045 ? srgb / 12.92 : std::pow((srgb + 0.055) / 1.055, 2.4);
            background[c] = -std::log(1.0 - mapped) / exposure.exposure;
        }
    }
    glClearColor(background.r, background.g, background.b, 1.0f);
    glStencilMask(0xff);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | (recordObjects ? GL_STENCIL_BUFFER_BIT : 0));
    drawSkybox(projection, view);
    drawOpaqueScene(projection, view, glm::vec3(0.0f), -1.0f, true);

    opaqueScope.stop();
    const bool hasWater = std::any_of(
        scenario.planets.begin(), scenario.planets.end(),
        [](const config::PlanetConfig& planet) { return planet.water.enabled; });
    const auto finishFrame = [&]() {
        {
            Scope atmosphereScope(profiler, Stage::Atmosphere);
            if (hdr) exposure.exposure = atmosphere.finish(atmosphereShader, scenario, bodies, lighting,
                exposure.exposure, view, projection, eyeWorld, outputFramebuffer, true, &atmosphereColumns, protectHighlights, &shadows);
        }
        if (protectHighlights && !hdr) {
            // Preserve legacy display-space water blending and MSAA when the
            // frame already meets the limit. Only overexposed airless frames
            // need a second render in HDR to recover their lost highlights.
            std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 4);
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            std::size_t clipped = 0;
            for (std::size_t i = 0; i < pixels.size(); i += 4)
                if (pixels[i] >= 250 && pixels[i + 1] >= 250 && pixels[i + 2] >= 250) ++clipped;
            if (clipped > pixels.size() / 4 / 20)
                return renderScene(scenario, bodies, view, fov, eyeWorld, shader, waterShader, skyboxShader,
                    reflectionTarget, shadowShader, shadows, atmosphereShader, atmosphere, reflectionAtmosphere,
                    atmosphereColumns, sunMesh, skyboxMesh, planetMeshes, waterMeshes, width, height,
                    clip, meteredPlanet, recordObjects, profiler, true, outputFramebuffer, grass, sceneTime);
        }
        return exposure;
    };
    if (!hasWater) return finishFrame();
    reflectionTarget.ensure(width, height, hdr);

    // The opaque main-scene depth buffer masks this translucent sea. Each
    // planet reuses one bounded reflection target before drawing its water.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    for (std::size_t i = 0; i < scenario.planets.size(); ++i) {
        const auto& planet = scenario.planets[i];
        if (!planet.water.enabled) continue;
        const glm::dvec3 centerWorld = bodies[i + 1].position;
        const glm::vec3 center(centerWorld);
        const double radiusWorld =
            planet.radius + planet.water.level_m / scenario.metersPerWorldUnit();
        const glm::mat4 reflectedView = rendering::waterReflectionView(
            view, eyeWorld, centerWorld, radiusWorld);
        const glm::mat4 reflectedProjection = rendering::perspectiveProjection(
            fov, static_cast<float>(reflectionTarget.width()) /
                     reflectionTarget.height(), clip);
        const glm::mat4 reflectionViewProjection = reflectedProjection * reflectedView;

        Scope reflectionScope(profiler, Stage::Reflection);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        if (hdr) reflectionAtmosphere.begin(reflectionTarget.width(), reflectionTarget.height());
        else reflectionTarget.bind();
        glViewport(0, 0, reflectionTarget.width(), reflectionTarget.height());
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        drawSkybox(reflectedProjection, reflectedView);
        drawOpaqueScene(reflectedProjection, reflectedView, center,
                        static_cast<float>(radiusWorld));

        reflectionScope.stop();
        if (hdr) {
            Scope reflectionAirScope(profiler, Stage::ReflectionAtmosphere);
            const glm::dvec3 normal = glm::normalize(eyeWorld - centerWorld);
            const glm::dvec3 reflectedEye = rendering::reflectPointAcrossPlane(eyeWorld,
                centerWorld + normal * radiusWorld, normal);
            reflectionAtmosphere.finish(atmosphereShader, scenario, bodies, lighting, exposure.exposure,
                reflectedView, reflectedProjection, reflectedEye, reflectionTarget.framebuffer(), false, &atmosphereColumns, false, &shadows);
        }
        Scope waterScope(profiler, Stage::Water);
        glBindFramebuffer(GL_FRAMEBUFFER, sceneFramebuffer);
        glViewport(0, 0, width, height);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        waterShader.use();
        setBodyLighting(waterShader, i, static_cast<float>(radiusWorld / planet.radius));
        waterShader.setMat4("projection", glm::value_ptr(projection));
        waterShader.setMat4("view", glm::value_ptr(view));
        waterShader.setMat4("uReflectionViewProjection",
                            glm::value_ptr(reflectionViewProjection));
        waterShader.setFloat3("uCameraPosition", static_cast<float>(eyeWorld.x),
                              static_cast<float>(eyeWorld.y),
                              static_cast<float>(eyeWorld.z));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, reflectionTarget.colorTexture());
        waterShader.setInt("uReflectionTexture", 0);

        const glm::mat4 model = rendering::sphereModel(
            center, static_cast<float>(radiusWorld), glm::mat3(bodies[i + 1].orientation));
        waterShader.setMat4("model", glm::value_ptr(model));
        waterShader.setFloat3("uPlanetCenter", center.x, center.y, center.z);
        waterShader.setFloat3("uWaterColor", static_cast<float>(planet.water.color[0]),
                              static_cast<float>(planet.water.color[1]),
                              static_cast<float>(planet.water.color[2]));
        waterShader.setFloat("uOpacity", static_cast<float>(planet.water.opacity));
        waterShader.setFloat("uReflectionFraction",
                             static_cast<float>(planet.water.reflection_fraction));
        waterMeshes[i].draw();
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    return finishFrame();
}

}
