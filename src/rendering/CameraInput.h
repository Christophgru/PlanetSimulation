#pragma once

#include "rendering/OrbitCamera.h"
#include "rendering/PlanetSurfaceCamera.h"

enum class CameraMode { Orbit, Surface };

struct WalkKeys {
    bool forward = false;
    bool backward = false;
    bool left = false;
    bool right = false;
};

class CameraInput {
public:
    CameraInput(OrbitCamera& orbitCamera, PlanetSurfaceCamera* surfaceCamera)
        : camera_(orbitCamera), surface_(surfaceCamera) {}

    CameraMode mode() const { return mode_; }
    bool dragging() const { return dragging_; }
    bool autoActivated() const { return autoActivated_; }

    void selectOrbit() {
        mode_ = CameraMode::Orbit;
        dragging_ = false;
        pointerKnown_ = false;
        autoActivated_ = false;
        // An explicit orbit selection takes priority until the camera leaves
        // the proximity zone and subsequently enters it again.
        suppressAuto_ = nearPlanet();
    }

    void selectSurface() {
        if (surface_) {
            mode_ = CameraMode::Surface;
            dragging_ = false;
            pointerKnown_ = false;
            autoActivated_ = false;
        }
    }

    void beginDrag(double x, double y) {
        if (mode_ != CameraMode::Orbit) return;
        dragging_ = true;
        lastX_ = x;
        lastY_ = y;
    }

    void endDrag() { dragging_ = false; }

    void moveCursor(double x, double y) {
        if (mode_ == CameraMode::Surface && surface_) {
            if (pointerKnown_) surface_->look(x - lastX_, y - lastY_);
            pointerKnown_ = true;
        } else if (mode_ == CameraMode::Orbit && dragging_) {
            camera_.orbit(static_cast<float>(x - lastX_),
                          static_cast<float>(y - lastY_));
        } else {
            return;
        }
        lastX_ = x;
        lastY_ = y;
    }

    void scroll(double yOffset) {
        if (mode_ == CameraMode::Orbit) camera_.zoom(static_cast<float>(yOffset));
    }

    void update(const WalkKeys& keys, double elapsedSeconds) {
        if (mode_ == CameraMode::Orbit && surface_) {
            if (!nearPlanet()) {
                suppressAuto_ = false;
            } else if (!suppressAuto_) {
                surface_->enterFromWorld(glm::dvec3(camera_.position),
                                         glm::dvec3(camera_.target));
                mode_ = CameraMode::Surface;
                dragging_ = false;
                pointerKnown_ = false;
                autoActivated_ = true;
            }
        }
        if (mode_ == CameraMode::Surface && surface_) {
            surface_->walk(static_cast<int>(keys.forward) - static_cast<int>(keys.backward),
                           static_cast<int>(keys.right) - static_cast<int>(keys.left),
                           elapsedSeconds);
        }
    }

private:
    bool nearPlanet() const {
        if (!surface_) return false;
        const auto& frame = surface_->frame();
        const double distance = glm::length(glm::dvec3(camera_.position) - frame.center());
        return distance <= 1.1 * (2.0 * frame.radius());
    }

    OrbitCamera& camera_;
    PlanetSurfaceCamera* surface_;
    CameraMode mode_ = CameraMode::Orbit;
    bool dragging_ = false;
    bool pointerKnown_ = false;
    bool suppressAuto_ = false;
    bool autoActivated_ = false;
    double lastX_ = 0.0;
    double lastY_ = 0.0;
};
