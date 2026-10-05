#pragma once
#include "rendering/runtime/terrain/reload/ReloadTracking.h"
#include "app/scene/PreparedScene.h"
namespace rendering {
struct ReloadCommitState {
    double time=0,wall=0,walkSpeed=0;
    ClipPlanes clip;
    std::optional<glm::dvec3> characterEye;
};
// All fallible scalar/camera checks happen before the complete scene exchange.
ReloadCommitState prepareReloadCommit(const app::PreparedScene& scene,bool third,double time,
    std::optional<glm::dvec3> characterEye);
}
