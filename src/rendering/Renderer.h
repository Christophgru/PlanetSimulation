#pragma once
#include <memory>
namespace app { struct CommandLineOptions; }

namespace rendering {
// Construct/run/destroy on one thread, with at most one active Renderer.
// Construction initializes the scene, window, and GPU resources; destruction
// joins terrain jobs and releases GPU resources before destroying the context.
class Renderer {
public:
    explicit Renderer(app::CommandLineOptions options);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    int run();
    // Reload the watched config/replay in the current context. Capture compute
    // explicitly waits for replacement readiness; failures retain the scene.
    void reload();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
