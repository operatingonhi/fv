#pragma once
#include "graphics/scene.hpp"
#include "graphics/camera.hpp"
#include <span>
#include <memory>

struct ImFontAtlas;

namespace spatial {
class Renderer {
public:
    explicit Renderer(std::span<const SceneBox> boxes,Vec3 grid_origin={},std::span<const SceneLabel> labels={});
    ~Renderer();
    Renderer(const Renderer&)=delete;
    Renderer& operator=(const Renderer&)=delete;
    void set_scene(std::span<const SceneBox> boxes,Vec3 grid_origin={},std::span<const SceneLabel> labels={});
    void draw(const Camera& camera,int width,int height,int left=0) const;
private:
    void release();
    unsigned program_=0, vao_=0, buffer_=0;
    int matrix_location_=-1, vertex_count_=0;
    unsigned text_program_=0, text_vao_=0, text_buffer_=0, text_texture_=0;
    int text_matrix_location_=-1, text_vertex_count_=0;
    std::unique_ptr<ImFontAtlas> text_atlas_;
};
}
