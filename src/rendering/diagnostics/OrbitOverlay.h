#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "rendering/Shader.h"
#include "rendering/geometry/OrbitPaths.h"

namespace rendering {

class OrbitOverlay {
public:
    OrbitOverlay()
        : lineShader_("shaders/diagnostics/orbit_lines.vert", "shaders/diagnostics/orbit_lines.frag"),
          textShader_("shaders/diagnostics/performance_overlay.vert", "shaders/diagnostics/performance_overlay.frag") {
        glGenVertexArrays(1, &lineVao_); glGenBuffers(1, &lineBuffer_);
        glBindVertexArray(lineVao_); glBindBuffer(GL_ARRAY_BUFFER, lineBuffer_);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
        glEnableVertexAttribArray(0);
        glGenVertexArrays(1, &textVao_); glGenBuffers(1, &textBuffer_);
        glBindVertexArray(textVao_); glBindBuffer(GL_ARRAY_BUFFER, textBuffer_);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5*sizeof(float), nullptr);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5*sizeof(float), reinterpret_cast<void*>(2*sizeof(float)));
        glEnableVertexAttribArray(0); glEnableVertexAttribArray(1);
        glBindVertexArray(0);
    }
    ~OrbitOverlay() {
        glDeleteBuffers(1, &lineBuffer_); glDeleteVertexArrays(1, &lineVao_);
        glDeleteBuffers(1, &textBuffer_); glDeleteVertexArrays(1, &textVao_);
        glDeleteProgram(lineShader_.id); glDeleteProgram(textShader_.id);
    }
    OrbitOverlay(const OrbitOverlay&) = delete;
    OrbitOverlay& operator=(const OrbitOverlay&) = delete;

    void paths(const std::vector<OrbitTrail>& trails, const std::vector<glm::vec3>& colors,
               const glm::mat4& viewProjection) {
        if (trails.empty()) return;
        glDisable(GL_BLEND); glDisable(GL_STENCIL_TEST);
        glEnable(GL_DEPTH_TEST); glDepthMask(GL_FALSE);
        lineShader_.use(); lineShader_.setMat4("uViewProjection", glm::value_ptr(viewProjection));
        glBindVertexArray(lineVao_);
        for (std::size_t i = 0; i < trails.size(); ++i) {
            std::vector<glm::vec3> points;
            points.reserve(trails[i].positions.size());
            for (const auto& position : trails[i].positions) points.emplace_back(position);
            const glm::vec3 color = glm::mix(colors.at(i), glm::vec3(1.0f), 0.35f);
            lineShader_.setFloat3("uColor", color.r, color.g, color.b);
            glBindBuffer(GL_ARRAY_BUFFER, lineBuffer_);
            glBufferData(GL_ARRAY_BUFFER, points.size()*sizeof(glm::vec3), points.data(), GL_DYNAMIC_DRAW);
            glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(points.size()));
        }
        glBindVertexArray(0);
        glDepthMask(GL_TRUE);
    }

    void labels(const config::ScenarioConfig& scenario, const std::vector<simulation::BodyState>& bodies,
                const simulation::OrbitalSystem& system, const std::vector<glm::vec3>& colors,
                const glm::mat4& viewProjection, int width, int height) {
        if (width <= 0 || height <= 0) return;
        std::vector<float> vertices;
        struct Rect { float x, y, w, h; };
        std::vector<Rect> occupied;
        for (std::size_t i = 0; i < scenario.planets.size(); ++i) {
            const glm::vec4 clip = viewProjection * glm::vec4(bodies[i+1].position, 1.0);
            if (clip.w <= 0.0f) continue;
            const glm::vec3 ndc = glm::vec3(clip) / clip.w;
            if (std::abs(ndc.x) > 1.0f || std::abs(ndc.y) > 1.0f || std::abs(ndc.z) > 1.0f) continue;
            const auto& planet = scenario.planets[i];
            char line[128];
            std::vector<std::string> lines;
            lines.push_back(upper(planet.name + " < " + planet.orbit.parent));
            std::snprintf(line, sizeof(line), "A %.2G B %.2G %s", planet.orbit.semi_major_axis,
                          planet.orbit.semi_minor_axis, scenario.distance_unit.c_str());
            lines.emplace_back(line);
            std::snprintf(line, sizeof(line), "E %.3G T %.3G H", planet.orbit.eccentricity(),
                          system.periodSeconds(i+1) / 3600.0);
            lines.emplace_back(line);
            std::snprintf(line, sizeof(line), "R %.3G %s", planet.radius, scenario.distance_unit.c_str());
            lines.emplace_back(line);
            std::snprintf(line, sizeof(line), "M %.3G KG", planet.mass_kg);
            lines.emplace_back(line);
            const std::size_t longest = std::max_element(lines.begin(), lines.end(),
                [](const auto& a, const auto& b) { return a.size() < b.size(); })->size();
            const float panelWidth = static_cast<float>(longest * 6 + 14);
            const float panelHeight = static_cast<float>(lines.size() * 10 + 10);
            const float sx = (ndc.x + 1.0f) * width * 0.5f;
            const float sy = (1.0f - ndc.y) * height * 0.5f;
            Rect panel{std::clamp(sx + 12.0f, 2.0f, std::max(2.0f, width-panelWidth-2.0f)),
                       std::clamp(sy - panelHeight*0.5f, 2.0f, std::max(2.0f, height-panelHeight-2.0f)),
                       panelWidth, panelHeight};
            // Keep adjacent moon and planet cards from covering each other.
            for (int attempt = 0; attempt < 12; ++attempt) {
                bool overlaps = false;
                for (const auto& other : occupied)
                    if (panel.x < other.x+other.w && panel.x+panel.w > other.x &&
                        panel.y < other.y+other.h && panel.y+panel.h > other.y) overlaps = true;
                if (!overlaps) break;
                panel.y = std::fmod(panel.y + panelHeight + 4.0f,
                                    std::max(1.0f, static_cast<float>(height)-panelHeight));
            }
            occupied.push_back(panel);
            rectangle(vertices, panel.x, panel.y, panel.w, panel.h, {0.85f,0.85f,0.85f});
            rectangle(vertices, panel.x+1, panel.y+1, panel.w-2, panel.h-2, {0.015f,0.018f,0.03f});
            rectangle(vertices, panel.x+1, panel.y+1, panel.w-2, 3, colors.at(i));
            for (std::size_t row = 0; row < lines.size(); ++row) {
                float x = panel.x + 7.0f;
                const float y = panel.y + 7.0f + row*10.0f;
                for (char ch : lines[row]) {
                    const auto glyph = glyphRows(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
                    for (int gy = 0; gy < 7; ++gy) for (int gx = 0; gx < 5; ++gx)
                        if (glyph[gy] & (1 << (4-gx)))
                            rectangle(vertices, x+gx, y+gy, 1, 1, {0.95f,0.95f,0.95f});
                    x += 6.0f;
                }
            }
        }
        if (vertices.empty()) return;
        glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE);
        glDisable(GL_BLEND); glDisable(GL_STENCIL_TEST);
        textShader_.use(); textShader_.setFloat2("uViewport", width, height);
        glBindVertexArray(textVao_); glBindBuffer(GL_ARRAY_BUFFER, textBuffer_);
        glBufferData(GL_ARRAY_BUFFER, vertices.size()*sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()/5));
        glBindVertexArray(0); glDepthMask(GL_TRUE); glEnable(GL_DEPTH_TEST);
    }
private:
    Shader lineShader_, textShader_;
    GLuint lineVao_=0, lineBuffer_=0, textVao_=0, textBuffer_=0;
    static std::string upper(std::string value) {
        for (char& c : value) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return value;
    }
    static void rectangle(std::vector<float>& out, float x, float y, float w, float h,
                          const glm::vec3& color) {
        for (const auto& point : {glm::vec2{x,y}, {x+w,y}, {x,y+h}, {x,y+h}, {x+w,y}, {x+w,y+h}})
            out.insert(out.end(), {point.x, point.y, color.r, color.g, color.b});
    }
    static std::array<unsigned char,7> glyphRows(char c) {
        switch (c) {
        case 'A': return {14,17,17,31,17,17,17}; case 'B': return {30,17,17,30,17,17,30};
        case 'C': return {14,17,16,16,16,17,14}; case 'D': return {30,17,17,17,17,17,30};
        case 'E': return {31,16,16,30,16,16,31}; case 'F': return {31,16,16,30,16,16,16};
        case 'G': return {14,17,16,23,17,17,14}; case 'H': return {17,17,17,31,17,17,17};
        case 'I': return {31,4,4,4,4,4,31}; case 'J': return {7,2,2,2,18,18,12};
        case 'K': return {17,18,20,24,20,18,17}; case 'L': return {16,16,16,16,16,16,31};
        case 'M': return {17,27,21,21,17,17,17}; case 'N': return {17,25,25,21,19,19,17};
        case 'O': return {14,17,17,17,17,17,14}; case 'P': return {30,17,17,30,16,16,16};
        case 'Q': return {14,17,17,17,21,18,13}; case 'R': return {30,17,17,30,20,18,17};
        case 'S': return {15,16,16,14,1,1,30}; case 'T': return {31,4,4,4,4,4,4};
        case 'U': return {17,17,17,17,17,17,14}; case 'V': return {17,17,17,17,17,10,4};
        case 'W': return {17,17,17,21,21,21,10}; case 'X': return {17,17,10,4,10,17,17};
        case 'Y': return {17,17,10,4,4,4,4}; case 'Z': return {31,1,2,4,8,16,31};
        case '0': return {14,17,19,21,25,17,14}; case '1': return {4,12,4,4,4,4,14};
        case '2': return {14,17,1,2,4,8,31}; case '3': return {30,1,1,14,1,1,30};
        case '4': return {2,6,10,18,31,2,2}; case '5': return {31,16,16,30,1,1,30};
        case '6': return {14,16,16,30,17,17,14}; case '7': return {31,1,2,4,8,8,8};
        case '8': return {14,17,17,14,17,17,14}; case '9': return {14,17,17,15,1,1,14};
        case '.': return {0,0,0,0,0,12,12}; case '-': return {0,0,0,31,0,0,0};
        case '+': return {0,4,4,31,4,4,0}; case '<': return {2,4,8,16,8,4,2};
        case '/': return {1,1,2,4,8,16,16}; default: return {};
        }
    }
};

} // namespace rendering
