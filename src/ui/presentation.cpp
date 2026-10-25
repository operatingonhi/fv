#include "ui/presentation.hpp"
#include <algorithm>

namespace spatial {
Vec3 semantic_color(Category category) {
    switch (category) {
    case Category::Directory: return {0.25f,0.65f,1.0f};
    case Category::Source: return {0.95f,0.85f,0.55f};
    case Category::Text: return {0.78f,0.85f,0.84f};
    case Category::Image: return {0.9f,0.45f,0.85f};
    case Category::Media: return {0.7f,0.5f,0.95f};
    case Category::Archive: return {1.0f,0.4f,0.35f};
    case Category::Executable: return {0.35f,0.95f,0.5f};
    case Category::Binary: return {0.45f,0.7f,0.45f};
    case Category::Generated: return {0.45f,0.5f,0.55f};
    case Category::Symlink: return {0.3f,0.9f,0.9f};
    case Category::Special: return {0.7f,0.65f,0.55f};
    case Category::Mixed: return {0.4f,0.8f,0.8f};
    }
    return {1,1,1};
}
namespace {
void file_geometry(std::vector<SceneBox>& boxes,FileShape shape,Vec3 center,Vec3 size,Vec3 color) {
    const float ground=center.y-size.y/2;
    const auto box=[&](Vec3 offset,Vec3 proportions) {
        boxes.push_back({center+Vec3{offset.x*size.x,offset.y*size.y,offset.z*size.z},
            {size.x*proportions.x,size.y*proportions.y,size.z*proportions.z},color});
    };
    switch (shape) {
    case FileShape::Document:
        box({0,0,0},{0.65f,1,0.16f});
        box({0,0.22f,0.09f},{0.48f,0.035f,0.02f});
        break;
    case FileShape::ImagePanel:
        box({0,0,0},{1,0.6f,0.12f});
        box({0,-0.32f,0},{0.45f,0.08f,0.45f});
        break;
    case FileShape::MediaPanels:
        box({-0.15f,0,-0.12f},{0.65f,0.75f,0.12f});
        box({0.15f,-0.12f,0.12f},{0.65f,0.75f,0.12f});
        break;
    case FileShape::ArchiveStack:
        for (int n=0;n<3;++n) box({0,-0.32f+n*0.32f,0},{0.85f,0.23f,0.8f});
        break;
    case FileShape::Linked:
        box({-0.17f,-0.05f,-0.1f},{0.6f,0.45f,0.35f});
        box({0.17f,0.05f,0.1f},{0.6f,0.45f,0.35f});
        break;
    case FileShape::LowTile:
        boxes.push_back({{center.x,ground+size.y*0.08f,center.z},{size.x,size.y*0.16f,size.z*0.8f},color});
        break;
    default: box({0,0,0},{0.8f,0.85f,0.75f}); break;
    }
}
void enclosure(std::vector<SceneBox>& boxes,LocalTransform transform,GroupKind group,Vec3 color) {
    const auto box=[&](Vec3 position,Vec3 size) {
        boxes.push_back({transform.point(position),size*transform.scale,color});
    };
    box(diorama_focus+Vec3{0,-0.05f,0},{16,0.1f,16});
    if (group==GroupKind::None) {
        box({0,1.2f,-10.45f},{16,2.4f,0.1f});
    } else {
        // A shared floor, roof frame and supports join the actual member modules.
        // Opening the group zooms into these same units; it never scatters them.
        box(diorama_focus+Vec3{0,1.6f,0},{15.6f,0.06f,15.6f});
        box({-7.8f,0.8f,-10.3f},{0.08f,1.6f,0.08f});
        box({7.8f,0.8f,-10.3f},{0.08f,1.6f,0.08f});
    }
}
}
void detail_levels(const Hierarchy& hierarchy,const Camera& camera,std::vector<unsigned char>& result) {
    result.assign(hierarchy.size(),0);
    const auto eye=camera.eye();
    for (auto id:hierarchy.visible_nodes()) {
        const auto offset=eye-hierarchy.focus(id);
        const auto ratio=hierarchy.node(id).transform.scale/std::max(0.0000001f,std::sqrt(dot(offset,offset)));
        result[id]=ratio>0.018f ? 2:ratio>0.002f ? 1:0;
    }
}
Presentation present(const Hierarchy& hierarchy,NodeId current,std::size_t selected,
    std::span<const unsigned char> levels) {
    Presentation result;
    std::vector<NodeId> ancestors;
    for (auto id=current;id!=no_node;id=hierarchy.node(id).parent) ancestors.push_back(id);
    enclosure(result.boxes,hierarchy.node(0).transform,GroupKind::None,semantic_color(Category::Directory)*0.6f);
    const auto visit=[&](auto&& self,NodeId id)->void {
        const auto& node=hierarchy.node(id);
        const auto& layout=node.world;
        const std::size_t limit=levels[id]==2 ? layout.objects.size():levels[id]==1 ? 6:3;
        for (std::size_t i=0;i<layout.objects.size();++i) {
            const auto child=hierarchy.child(id,i);
            const bool active=id==current && i==selected;
            const bool on_path=std::find(ancestors.begin(),ancestors.end(),child)!=ancestors.end();
            const auto& object=layout.objects[i];
            const auto color=semantic_color(object.item.category)*(0.5f+0.5f*object.item.salience);
            if (id==current) result.targets.push_back({object.center,object.size,color});
            if (i>=limit && !active && !on_path) continue;
            const bool container=object.item.category==Category::Directory || object.item.group!=GroupKind::None;
            if (container) {
                const auto transform=compose(node.transform,contained_transform(node.local.objects[i]));
                enclosure(result.boxes,transform,object.item.group,color);
                if (child!=no_node) self(self,child);
            } else file_geometry(result.boxes,file_shape(object.item.category),object.center,object.size,color);
            if (active)
                result.boxes.push_back({object.center,object.size+Vec3{0.12f,0.12f,0.12f}*node.transform.scale,{1,0.95f,0.5f}});
            std::string label=object.item.label;
            if (object.item.group!=GroupKind::None) label+=" ("+std::to_string(object.item.members.size())+")";
            result.labels.push_back({object.center+Vec3{0,object.size.y/2+0.2f*node.transform.scale,0},
                active ? Vec3{1,0.95f,0.5f}:color,std::move(label),i<7 || active || container,active,object.size.x});
        }
    };
    visit(visit,0);
    return result;
}
}
