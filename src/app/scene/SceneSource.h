#pragma once
#include "config/Config.h"
#include "app/CommandLineOptions.h"

namespace app {
// Config and replay validation complete before a window or GPU resources exist.
class SceneSource {
public:
    explicit SceneSource(CommandLineOptions& options);
    // Reload retains the resident pipeline while validating a fresh snapshot.
    static SceneSource forResidentReload(CommandLineOptions& options);
    nlohmann::json load() const;
    const std::string watchedScenePath;
    nlohmann::json document;
    nlohmann::json replayDocument;
private:
    std::string configPath;
    std::string replayPath;
    nlohmann::json documentFor(const nlohmann::json& replay) const;
};
}
