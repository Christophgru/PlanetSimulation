#pragma once

#include "rendering/OrbitCamera.h"
#include "rendering/PlanetSurfaceCamera.h"

enum class CameraMode { Orbit, Surface, PlanetOrbit };

struct WalkKeys {
    bool forward = false;
    bool backward = false;
    bool left = false;
    bool right = false;
};

class CameraInput {
public:
    CameraInput(OrbitCamera& orbitCamera, PlanetSurfaceCamera* surfaceCamera,
                OrbitCamera* planetOrbitCamera = nullptr)
        : camera_(orbitCamera), surface_(surfaceCamera), planetOrbit_(planetOrbitCamera) {}

    CameraMode mode() const { return mode_; }
    bool dragging() const { return dragging_; }
    bool autoActivated() const { return autoActivated_; }
    bool surfacePointerCaptured() const { return surfacePointerCaptured_; }

    void releaseCursor() {
        if (mode_ == CameraMode::Surface) {
            surfacePointerCaptured_ = false;
            pointerKnown_ = false;
        }
    }

    // The Sun orbit camera keeps its address; scene reload replaces the two
    // optional cameras and may remove either mode.
    void rebind(PlanetSurfaceCamera* surfaceCamera, OrbitCamera* planetOrbitCamera) {
        surface_ = surfaceCamera;
        planetOrbit_ = planetOrbitCamera;
        if ((mode_ == CameraMode::Surface && !surface_) ||
            (mode_ == CameraMode::PlanetOrbit && !planetOrbit_))
            mode_ = CameraMode::Orbit;
        if (mode_ != CameraMode::Surface) surfacePointerCaptured_ = false;
        dragging_ = false;
        pointerKnown_ = false;
        autoActivated_ = false;
        suppressAuto_ = activeOrbit() && nearPlanet(*activeOrbit());
    }

    void selectOrbit() {
        mode_ = CameraMode::Orbit;
        surfacePointerCaptured_ = false;
        dragging_ = false;
        pointerKnown_ = false;
        autoActivated_ = false;
        // An explicit orbit selection takes priority until the camera leaves
        // the proximity zone and subsequently enters it again.
        suppressAuto_ = nearPlanet(camera_);
    }

    void selectPlanetOrbit() {
        if (!planetOrbit_) return;
        if (mode_ == CameraMode::Surface && surface_)
            planetOrbit_->alignRadial(glm::vec3(
                surface_->position() - surface_->frame().center()));
        mode_ = CameraMode::PlanetOrbit;
        surfacePointerCaptured_ = false;
        dragging_ = false;
        pointerKnown_ = false;
        autoActivated_ = false;
        suppressAuto_ = nearPlanet(*planetOrbit_);
    }

    void selectSurface() {
        if (surface_) {
            mode_ = CameraMode::Surface;
            surfacePointerCaptured_ = true;
            dragging_ = false;
            pointerKnown_ = false;
            autoActivated_ = false;
        }
    }

    void beginDrag(double x, double y) {
        if (mode_ == CameraMode::Surface) return;
        dragging_ = true;
        lastX_ = x;
        lastY_ = y;
    }

    void endDrag() { dragging_ = false; }

    void moveCursor(double x, double y) {
        if (mode_ == CameraMode::Surface && surface_ && surfacePointerCaptured_) {
            if (pointerKnown_) surface_->look(x - lastX_, y - lastY_);
            pointerKnown_ = true;
        } else if (dragging_) {
            if (OrbitCamera* orbit = activeOrbit())
                orbit->orbit(static_cast<float>(x - lastX_),
                             static_cast<float>(y - lastY_));
        } else {
            return;
        }
        lastX_ = x;
        lastY_ = y;
    }

    void scroll(double yOffset) {
        if (OrbitCamera* orbit = activeOrbit())
            orbit->zoom(static_cast<float>(yOffset));
    }

    void update(const WalkKeys& keys, double elapsedSeconds) {
        if (OrbitCamera* orbit = activeOrbit()) {
            orbit->advance(elapsedSeconds);
        }
        if (OrbitCamera* orbit = activeOrbit(); orbit && surface_) {
            if (!nearPlanet(*orbit)) {
                suppressAuto_ = false;
            } else if (!suppressAuto_) {
                surface_->enterFromWorld(glm::dvec3(orbit->position),
                                         surface_->target());
                mode_ = CameraMode::Surface;
                surfacePointerCaptured_ = true;
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
    OrbitCamera* activeOrbit() const {
        if (mode_ == CameraMode::Orbit) return &camera_;
        if (mode_ == CameraMode::PlanetOrbit) return planetOrbit_;
        return nullptr;
    }

    bool nearPlanet(const OrbitCamera& orbit) const {
        if (!surface_) return false;
        const auto& frame = surface_->frame();
        const double distance = glm::length(glm::dvec3(orbit.position) - frame.center());
        return distance <= 1.1 * (2.0 * frame.radius());
    }

    OrbitCamera& camera_;
    PlanetSurfaceCamera* surface_;
    OrbitCamera* planetOrbit_;
    CameraMode mode_ = CameraMode::Orbit;
    bool dragging_ = false;
    bool pointerKnown_ = false;
    bool suppressAuto_ = false;
    bool autoActivated_ = false;
    bool surfacePointerCaptured_ = false;
    double lastX_ = 0.0;
    double lastY_ = 0.0;
};
