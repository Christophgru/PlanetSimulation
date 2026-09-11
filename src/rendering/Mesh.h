#pragma once

#include <vector>
#include <GL/gl.h>

class Mesh {
public:
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    
    Mesh() = default;
    
    void addVertex(float x, float y, float z, float nx, float ny, float nz) {
        vertices.push_back(x);
        vertices.push_back(y);
        vertices.push_back(z);
        vertices.push_back(nx);
        vertices.push_back(ny);
        vertices.push_back(nz);
    }
    
    void addTriangle(unsigned int idx0, unsigned int idx1, unsigned int idx2) {
        indices.push_back(idx0);
        indices.push_back(idx1);
        indices.push_back(idx2);
    }
    
    void generateSphere(int segments) {
        vertices.clear();
        indices.clear();
        
        // Generate sphere with proper triangle indices
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
                
                addVertex(x, y, z, nx, ny, nz);
            }
        }
        
        // Generate indices for triangle list
        for (int lat = 0; lat < segments; lat++) {
            for (int lon = 0; lon < segments; lon++) {
                unsigned int first = (lat * (segments + 1) + lon);
                unsigned int second = first + segments + 1;
                
                indices.push_back(first);
                indices.push_back(second);
                indices.push_back(first + 1);
                
                indices.push_back(second);
                indices.push_back(second + 1);
                indices.push_back(first + 1);
            }
        }
        
        // Upload to GPU
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
        
        // Position attribute (location 0)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        
        // Normal attribute (location 1)
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), 
                             (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        
        glBindVertexArray(0);
    }
    
    void draw() const {
        if (vao == 0 || vbo == 0 || ebo == 0) {
            std::cerr << "Mesh not initialized\n";
            return;
        }
        
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), 
                             (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);
        
        glBindVertexArray(0);
    }
};
