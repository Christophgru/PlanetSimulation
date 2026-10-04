#pragma once
#include "app/scene/PreparedScene.h"
#include "rendering/runtime/ResourceOwners.h"
#include "rendering/foliage/GrassRenderer.h"
#include "config/Config.h"

namespace rendering {
// CPU/legacy adapter owns both complete replacement and retired consumers.
// Grass buffer views die before meshes, with the context retained by Renderer.
struct LegacySceneReplacement {
    nlohmann::json document;
    app::PreparedScene scene;
    SceneMeshes meshes;
    GrassRenderer grass;
    GLsync retirementFence=nullptr;
    LegacySceneReplacement(nlohmann::json doc,app::PreparedScene prepared)
        :document(std::move(doc)),scene(std::move(prepared)),meshes(scene.scenario.planets.size()) {}
    ~LegacySceneReplacement(){if(retirementFence)glDeleteSync(retirementFence);}
    bool pollRetired() const {
        const auto status=glClientWaitSync(retirementFence,0,0);
        if(status==GL_WAIT_FAILED) throw std::runtime_error("Legacy scene retirement poll failed");
        return status!=GL_TIMEOUT_EXPIRED;
    }
    void waitForCapture() const {
        while(!pollRetired())
            if(glClientWaitSync(retirementFence,GL_SYNC_FLUSH_COMMANDS_BIT,1000000)==GL_WAIT_FAILED)
                throw std::runtime_error("Legacy scene retirement wait failed");
    }
};
}
