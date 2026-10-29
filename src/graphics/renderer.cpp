#define GL_GLEXT_PROTOTYPES
#include <SDL_opengl.h>
#include "graphics/renderer.hpp"
#include <imgui.h>
#include "graphics/inscriptions.hpp"
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace spatial {
namespace {
struct Vertex { Vec3 position, color; };
GLuint shader(GLenum type,const char* source) {
    const GLuint id=glCreateShader(type);
    glShaderSource(id,1,&source,nullptr);
    glCompileShader(id);
    GLint ok=0;
    glGetShaderiv(id,GL_COMPILE_STATUS,&ok);
    if (!ok) {
        char log[2048]{};
        glGetShaderInfoLog(id,sizeof(log),nullptr,log);
        glDeleteShader(id);
        throw std::runtime_error(std::string("Shader compilation: ")+log);
    }
    return id;
}
}
Renderer::Renderer(std::span<const SceneBox> boxes,Vec3 grid_origin,std::span<const SceneLabel> labels) {
    GLuint vertex=0, fragment=0;
    try {
        vertex=shader(GL_VERTEX_SHADER,R"(#version 330 core
layout(location=0) in vec3 position;
layout(location=1) in vec3 color;
uniform mat4 view_projection;
out vec3 tint;
void main() { tint=color; gl_Position=view_projection*vec4(position,1.0); }
)");
        fragment=shader(GL_FRAGMENT_SHADER,R"(#version 330 core
in vec3 tint;
out vec4 pixel;
void main() { pixel=vec4(tint,1.0); }
)");
        program_=glCreateProgram();
        glAttachShader(program_,vertex);
        glAttachShader(program_,fragment);
        glLinkProgram(program_);
        GLint ok=0;
        glGetProgramiv(program_,GL_LINK_STATUS,&ok);
        if (!ok) {
            char log[2048]{};
            glGetProgramInfoLog(program_,sizeof(log),nullptr,log);
            throw std::runtime_error(std::string("Shader linking: ")+log);
        }
        glDeleteShader(vertex); vertex=0;
        glDeleteShader(fragment); fragment=0;
        matrix_location_=glGetUniformLocation(program_,"view_projection");
        glGenVertexArrays(1,&vao_);
        glGenBuffers(1,&buffer_);
        vertex=shader(GL_VERTEX_SHADER,R"(#version 330 core
layout(location=0) in vec3 position;
layout(location=1) in vec3 color;
layout(location=2) in vec2 uv;
uniform mat4 view_projection;
out vec3 tint;
out vec2 texcoord;
void main() { tint=color; texcoord=uv; gl_Position=view_projection*vec4(position,1.0); }
)");
        fragment=shader(GL_FRAGMENT_SHADER,R"(#version 330 core
in vec3 tint;
in vec2 texcoord;
uniform sampler2D atlas;
out vec4 pixel;
void main() {
    float alpha=texture(atlas,texcoord).a;
    if (!gl_FrontFacing || alpha<0.05) discard;
    pixel=vec4(tint,alpha);
}
)");
        text_program_=glCreateProgram();
        glAttachShader(text_program_,vertex); glAttachShader(text_program_,fragment);
        glLinkProgram(text_program_);
        glGetProgramiv(text_program_,GL_LINK_STATUS,&ok);
        if (!ok) throw std::runtime_error("Inscription shader linking failed");
        glDeleteShader(vertex); vertex=0; glDeleteShader(fragment); fragment=0;
        text_matrix_location_=glGetUniformLocation(text_program_,"view_projection");
        glUseProgram(text_program_);
        glUniform1i(glGetUniformLocation(text_program_,"atlas"),0);
        glGenVertexArrays(1,&text_vao_); glGenBuffers(1,&text_buffer_);
        // A fixed atlas owned by the scene renderer avoids depending on the UI's
        // dynamic texture lifetime or rebuilding text as the camera moves.
        text_atlas_=std::make_unique<ImFontAtlas>();
        add_inscription_font(*text_atlas_);
        unsigned char* pixels=nullptr; int width=0,height=0;
        text_atlas_->GetTexDataAsRGBA32(&pixels,&width,&height);
        glGenTextures(1,&text_texture_); glBindTexture(GL_TEXTURE_2D,text_texture_);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
        glGenerateMipmap(GL_TEXTURE_2D);
        set_scene(boxes,grid_origin,labels);
    } catch (...) {
        if (vertex) glDeleteShader(vertex);
        if (fragment) glDeleteShader(fragment);
        release();
        throw;
    }
}
void Renderer::set_scene(std::span<const SceneBox> boxes,Vec3 grid_origin,std::span<const SceneLabel> labels) {
    std::vector<Vertex> vertices;
    vertices.reserve(116+boxes.size()*24);
    const auto line=[&](Vec3 a,Vec3 b,Vec3 color) {
        vertices.push_back({a,color}); vertices.push_back({b,color});
    };
    for (int i=-14;i<=14;++i) {
        const float f=static_cast<float>(i);
        line(grid_origin+Vec3{f,-0.02f,-14},grid_origin+Vec3{f,-0.02f,14},{0.09f,0.18f,0.20f});
        line(grid_origin+Vec3{-14,-0.02f,f},grid_origin+Vec3{14,-0.02f,f},{0.09f,0.18f,0.20f});
    }
    constexpr int edges[][2]={{0,1},{1,3},{3,2},{2,0},{4,5},{5,7},
                              {7,6},{6,4},{0,4},{1,5},{2,6},{3,7}};
    for (const auto& box:boxes) {
        Vec3 corners[8];
        for (int i=0;i<8;++i)
            corners[i]=box.center+Vec3{(i&1 ? 0.5f:-0.5f)*box.size.x,
                (i&2 ? 0.5f:-0.5f)*box.size.y,(i&4 ? 0.5f:-0.5f)*box.size.z};
        for (const auto& edge:edges) line(corners[edge[0]],corners[edge[1]],box.color);
    }
    vertex_count_=static_cast<int>(vertices.size());
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER,buffer_);
    glBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>(vertices.size()*sizeof(Vertex)),vertices.data(),GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(sizeof(Vec3)));
    glBindVertexArray(0);
    const auto text_vertices=inscription_mesh(*text_atlas_,labels);
    text_vertex_count_=static_cast<int>(text_vertices.size());
    glBindVertexArray(text_vao_); glBindBuffer(GL_ARRAY_BUFFER,text_buffer_);
    glBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>(text_vertices.size()*sizeof(InscriptionVertex)),text_vertices.data(),GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(InscriptionVertex),nullptr);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(InscriptionVertex),reinterpret_cast<void*>(offsetof(InscriptionVertex,color)));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(InscriptionVertex),reinterpret_cast<void*>(offsetof(InscriptionVertex,uv)));
    glBindVertexArray(0);
}
void Renderer::release() {
    if (text_texture_) glDeleteTextures(1,&text_texture_);
    if (text_buffer_) glDeleteBuffers(1,&text_buffer_);
    if (text_vao_) glDeleteVertexArrays(1,&text_vao_);
    if (text_program_) glDeleteProgram(text_program_);
    if (buffer_) glDeleteBuffers(1,&buffer_);
    if (vao_) glDeleteVertexArrays(1,&vao_);
    if (program_) glDeleteProgram(program_);
}
Renderer::~Renderer() { release(); }
void Renderer::draw(const Camera& camera,int width,int height,int left) const {
    if (width<=0 || height<=0) return;
    glViewport(left,0,width,height);
    glClearColor(0.018f,0.027f,0.040f,1);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glUseProgram(program_);
    const auto matrix=camera.view_projection(static_cast<float>(width)/static_cast<float>(height));
    glUniformMatrix4fv(matrix_location_,1,GL_FALSE,matrix.data());
    glBindVertexArray(vao_);
    glDrawArrays(GL_LINES,0,vertex_count_);
    glUseProgram(text_program_);
    glUniformMatrix4fv(text_matrix_location_,1,GL_FALSE,matrix.data());
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,text_texture_);
    glBindSampler(0,0);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glBindVertexArray(text_vao_);
    glDrawArrays(GL_TRIANGLES,0,text_vertex_count_);
    glDisable(GL_BLEND);
    glBindVertexArray(0);
}
}
