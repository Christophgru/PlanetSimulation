#pragma once

#include <array>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <string>
#include <stdexcept>
#include <GL/glew.h>

namespace rendering {
// Wall-clock presentation rate, averaged over a short window (not 1/CPU time).
class FrameRate {
public:
    void sample(double seconds) {
        if (!(seconds > 0) || seconds > 10) return;
        elapsed_ += seconds; ++frames_;
        if (elapsed_ >= 0.25) {
            milliseconds = 1000 * elapsed_ / frames_;
            fps = frames_ / elapsed_;
            elapsed_ = 0; frames_ = 0;
        }
    }
    double fps = 0, milliseconds = 0;
private:
    double elapsed_ = 0;
    unsigned frames_ = 0;
};

enum class FrameStage { Update, Mesh, Lighting, Tables, Shadows, Opaque, Reflection,
                        ReflectionAtmosphere, Water, Atmosphere, CachedPresentation, Overlay, Present, Count };
inline constexpr std::array<const char*, static_cast<int>(FrameStage::Count)> frameStageNames{
    "update", "mesh", "lighting", "tables", "shadows", "opaque", "reflection",
    "reflection_atmosphere", "water", "atmosphere", "cached_present", "overlay", "present"};

class FrameProfiler {
    using Clock = std::chrono::steady_clock;
    static constexpr int stageCount = static_cast<int>(FrameStage::Count);
    static constexpr int maxEvents = 63;
    struct Event { FrameStage stage; Clock::time_point start; bool gpu; };
    struct Frame {
        std::array<GLuint, 2 + 2 * maxEvents> queries{};
        std::array<Event, maxEvents> events{};
        std::array<double, stageCount> cpu{}, gpu{};
        Clock::time_point start;
        unsigned long long number = 0;
        double simulation = 0, wallMs = 0, gpuMs = 0, terrainBuildMs = 0;
        int count = 0, shadowUpdates = 0, shadowReuses = 0, meshUploads = 0, sceneReuses = 0;
        bool pending = false, timed = false;
    };
public:
    explicit FrameProfiler(const std::string& path = "") {
        if (path.empty()) return;
        trace_.open(path);
        if (!trace_) throw std::runtime_error("Cannot open performance trace: " + path);
        trace_ << "frame,simulation_s,frame_ms,gpu_valid,gpu_ms,shadow_updates,shadow_reuses,mesh_uploads,scene_reuses,terrain_build_ms";
        for (const auto* name : frameStageNames) trace_ << ",cpu_" << name << "_ms,gpu_" << name << "_ms";
        trace_ << '\n' << std::setprecision(9);
    }
    ~FrameProfiler() {
        collect();
        for (auto& frame : frames_) {
            if (frame.pending) write(frame, false); // Never wait for a busy GPU on shutdown.
            if (frame.queries[0]) glDeleteQueries(frame.queries.size(), frame.queries.data());
        }
    }
    FrameProfiler(const FrameProfiler&) = delete;
    FrameProfiler& operator=(const FrameProfiler&) = delete;
    void beginFrame(double simulation, bool showGpu = false) {
        collect();
        current_ = &fallback_;
        for (auto& frame : frames_) if (!frame.pending) { current_ = &frame; break; }
        const auto queries = current_->queries;
        *current_ = Frame{}; current_->queries = queries;
        current_->number = next_++; current_->simulation = simulation;
        current_->timed = current_ != &fallback_ && (trace_.is_open() || showGpu);
        current_->start = Clock::now();
        if (current_->timed) {
            if (!current_->queries[0]) glGenQueries(current_->queries.size(), current_->queries.data());
            glQueryCounter(current_->queries[0], GL_TIMESTAMP);
        }
    }
    void endFrame() {
        if (!current_) return;
        current_->wallMs = elapsed(current_->start);
        if (current_->timed) {
            glQueryCounter(current_->queries[1], GL_TIMESTAMP);
            current_->pending = true;
        } else write(*current_, false);
        current_ = nullptr;
    }
    void collect() {
        for (auto& frame : frames_) {
            if (!frame.pending) continue;
            GLint available = GL_FALSE;
            glGetQueryObjectiv(frame.queries[1], GL_QUERY_RESULT_AVAILABLE, &available);
            if (!available) continue; // No GL_QUERY_RESULT until the final timestamp is ready.
            frame.gpuMs = 0;
            for (int i = 0; i < frame.count; ++i) if (frame.events[i].gpu)
                frame.gpu[static_cast<int>(frame.events[i].stage)] += duration(frame.queries[2+2*i],frame.queries[3+2*i]);
            for (double milliseconds : frame.gpu) frame.gpuMs += milliseconds;
            if (!lastReady_ || frame.number >= lastNumber_) {
                gpuMilliseconds = frame.gpuMs; lastNumber_ = frame.number; lastReady_ = true;
            }
            write(frame, true); frame.pending = false;
        }
    }
    void simulationTime(double seconds) { if (current_) current_->simulation=seconds; }
    void sceneReuse() { if (current_) ++current_->sceneReuses; }
    void shadowUpdate() { if (current_) ++current_->shadowUpdates; }
    void shadowReuse() { if (current_) ++current_->shadowReuses; }
    void terrainBuild(double milliseconds) { if (current_) current_->terrainBuildMs += milliseconds; }
    void meshUpload() { if (current_) ++current_->meshUploads; }
    bool gpuReady() const { return lastReady_; }
    double gpuMilliseconds = 0;
    class Scope {
    public:
        Scope(FrameProfiler* owner, FrameStage stage, bool gpu = true) : owner_(owner) {
            if (owner_) token_ = owner_->beginStage(stage, gpu);
        }
        ~Scope() { stop(); }
        void stop() { if (owner_ && token_ >= 0) owner_->endStage(token_); owner_ = nullptr; }
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
    private:
        FrameProfiler* owner_;
        int token_ = -1;
    };
private:
    std::array<Frame, 8> frames_{};
    Frame fallback_{};
    Frame* current_ = nullptr;
    std::ofstream trace_;
    unsigned long long next_ = 0, lastNumber_ = 0;
    bool lastReady_ = false;
    static double elapsed(Clock::time_point start) {
        return std::chrono::duration<double, std::milli>(Clock::now()-start).count();
    }
    static double duration(GLuint start, GLuint end) {
        GLuint64 a = 0, b = 0;
        glGetQueryObjectui64v(start, GL_QUERY_RESULT, &a);
        glGetQueryObjectui64v(end, GL_QUERY_RESULT, &b);
        return b >= a ? (b-a)*1e-6 : 0;
    }
    int beginStage(FrameStage stage, bool gpu) {
        if (!current_ || current_->count == maxEvents) return -1;
        const int token = current_->count++;
        current_->events[token] = {stage, Clock::now(), gpu && current_->timed};
        if (current_->events[token].gpu) glQueryCounter(current_->queries[2+2*token],GL_TIMESTAMP);
        return token;
    }
    void endStage(int token) {
        auto& event = current_->events[token];
        current_->cpu[static_cast<int>(event.stage)] += elapsed(event.start);
        if (event.gpu) glQueryCounter(current_->queries[3+2*token],GL_TIMESTAMP);
    }
    void write(const Frame& frame, bool gpuValid) {
        if (!trace_.is_open()) return;
        trace_ << frame.number << ',' << frame.simulation << ',' << frame.wallMs << ',' << gpuValid << ',';
        if (gpuValid) trace_ << frame.gpuMs;
        trace_ << ',' << frame.shadowUpdates << ',' << frame.shadowReuses << ',' << frame.meshUploads << ',' << frame.sceneReuses << ',' << frame.terrainBuildMs;
        for (int i = 0; i < stageCount; ++i) {
            trace_ << ',' << frame.cpu[i] << ',';
            if (gpuValid) trace_ << frame.gpu[i];
        }
        trace_ << '\n'; // Buffered, not flushed per frame.
    }
};
} // namespace rendering
