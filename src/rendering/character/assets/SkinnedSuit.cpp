#include "rendering/character/assets/SkinnedSuit.h"
#include <cstddef>
namespace rendering {
SkinnedSuit::SkinnedSuit():bind_(AcesSuit::load("data/character/aces/suit.bin")) {
    count_=static_cast<GLsizei>(bind_.indices.size());
    glGenVertexArrays(1,&vao_); glGenBuffers(1,&vbo_); glGenBuffers(1,&ebo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER,vbo_);
    glBufferData(GL_ARRAY_BUFFER,bind_.vertices.size()*sizeof(SuitVertex),bind_.vertices.data(),GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,bind_.indices.size()*sizeof(std::uint32_t),bind_.indices.data(),GL_STATIC_DRAW);
    const auto attribute=[](GLuint location,GLint size,std::size_t offset) {
        glEnableVertexAttribArray(location);
        glVertexAttribPointer(location,size,GL_FLOAT,GL_FALSE,sizeof(SuitVertex),reinterpret_cast<void*>(offset));
    };
    attribute(0,3,offsetof(SuitVertex,position)); attribute(1,3,offsetof(SuitVertex,normal));
    attribute(2,4,offsetof(SuitVertex,color));
    glEnableVertexAttribArray(3);
    glVertexAttribIPointer(3,2,GL_UNSIGNED_SHORT,sizeof(SuitVertex),reinterpret_cast<void*>(offsetof(SuitVertex,joints)));
    attribute(4,2,offsetof(SuitVertex,weights));
    glBindVertexArray(0);
    // Keep only the tiny bind rig on the CPU after the one-time upload.
    std::vector<SuitVertex>().swap(bind_.vertices);
    std::vector<std::uint32_t>().swap(bind_.indices);
}
SkinnedSuit::~SkinnedSuit() {
    glDeleteBuffers(1,&ebo_); glDeleteBuffers(1,&vbo_); glDeleteVertexArrays(1,&vao_);
}
void SkinnedSuit::draw() const {
    glBindVertexArray(vao_); glDrawElements(GL_TRIANGLES,count_,GL_UNSIGNED_INT,nullptr); glBindVertexArray(0);
}
}
