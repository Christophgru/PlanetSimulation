#pragma once

#include <vector>
#include <GL/gl.h>

class Mesh {
public:
    GLuint vao, vbo, ebo;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    
    Mesh() : vao(0), vbo(0), ebo(0) {}
    
    void addVertex(float x, float y, float z, float nx, float ny, float nz, 
                   float tx, float ty, float tz = 0.0f, unsigned int texCoord = 0) {
        vertices.push_back(x);
        vertices.push_back(y);
        vertices.push_back(z);
        vertices.push_back(nx);
        vertices.push_back(ny);
        vertices.push_back(nz);
        vertices.push_back(tx);
        vertices.push_back(ty);
        vertices.push_back(tz);
        vertices.push_back(texCoord);
    }
    
    void addTriangle(unsigned int idx0, unsigned int idx1, unsigned int idx2) {
        indices.push_back(idx0);
        indices.push_back(idx1);
        indices.push_back(idx2);
    }
    
    void generateSphere(int segments) {
        vertices.clear();
        indices.clear();
        
        float vCount = (segments + 1) * (segments + 1);
        std::vector<float> tempVertices(vCount * 3);
        
        for (int lat = 0; lat <= segments; lat++) {
            for (int lon = 0; lon <= segments; lon++) {
                float theta = M_PI * 2.0f * lon / segments;
                float phi = M_PI * lat / segments;
                
                float x = cos(theta) * sin(phi);
                float y = cos(phi);
                float z = sin(theta) * sin(phi);
                
                // Normal is same as position for unit sphere
                float nx = x;
                float ny = y;
                float nz = z;
                
                addVertex(x, y, z, nx, ny, nz, 0.0f, 0.0f, 0.0f, 0);
            }
        }
        
        // Generate indices for sphere triangles
        for (int lat = 0; lat <= segments; lat++) {
            for (int lon = 0; lon <= segments; lon++) {
                unsigned int first = (lat * (segments + 1) + lon);
                unsigned int second = first + segments + 1;
                
                if (lon < segments && lat < segments) {
                    addTriangle(first, second, first + 1);
                    addTriangle(second, second + 1, first + 1);
                } else if (lon < segments) {
                    addTriangle(first, second, first + 1);
                } else if (lat < segments) {
                    addTriangle(first, second, first + 1);
                    addTriangle(second, second + 1, first + 1);
                }
            }
        }
        
        vertices.resize(vertices.size());
        indices.resize(indices.size());
        
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);
        
        glBindVertexArray(vao);
        
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), 
                     vertices.data(), GL_STATIC_DRAW);
        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
                     indices.data(), GL_STATIC_DRAW);
        
        // Position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 10 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        
        // Normal attribute
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 10 * sizeof(float), 
                             (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        
        // TexCoord attribute
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 10 * sizeof(float),
                             (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);
        
        glBindVertexArray(0);
    }
    
    void draw() const {
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
};
