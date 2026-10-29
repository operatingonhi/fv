#pragma once
#include "graphics/scene.hpp"
#include "graphics/camera.hpp"
#include "graphics/grid.hpp"
#include <span>
#include <memory>

struct ImFontAtlas;

namespace spatial {
class Renderer {
public:
    explicit Renderer(std::span<const SceneBox> boxes,GridFrame grid={},std::span<const SceneLabel> labels={});
    ~Renderer();
    Renderer(const Renderer&)=delete;
    Renderer& operator=(const Renderer&)=delete;
    void set_scene(std::span<const SceneBox> boxes,GridFrame grid={},std::span<const SceneLabel> labels={});
    void draw(const Camera& camera,int width,int height,int left=0) const;
private:
    void release();
    unsigned program_=0, vao_=0, buffer_=0;
    int matrix_location_=-1, vertex_count_=0;
    GridFrame grid_;
    unsigned grid_program_=0,grid_vao_=0;
    int grid_matrix_=-1,grid_center_=-1,grid_extent_=-1,grid_spacing_=-1,grid_phase_=-1,grid_opacity_=-1;
    unsigned text_program_=0, text_vao_=0, text_buffer_=0, text_texture_=0;
    int text_matrix_location_=-1, text_vertex_count_=0;
    std::unique_ptr<ImFontAtlas> text_atlas_;
};
}
