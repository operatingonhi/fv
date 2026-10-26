#include "ui/presentation.hpp"
#include <algorithm>

namespace spatial {
std::string display_name(std::string_view name) {
    std::string result;
    result.reserve(name.size());
    constexpr char hex[]="0123456789abcdef";
    for (unsigned char c:name) {
        if (c<32 || c==127) {
            result+="\\x"; result+=hex[c>>4]; result+=hex[c&15];
        } else result+=static_cast<char>(c);
    }
    return result;
}
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
    case FileShape::CodeSlab:
        box({0,0,0},{0.8f,0.85f,0.18f});
        box({-0.28f,0,0.12f},{0.06f,0.65f,0.12f});
        break;
    case FileShape::ExecutableFrame:
        box({-0.3f,0,0},{0.2f,1,0.6f});
        box({0.3f,0,0},{0.2f,1,0.6f});
        box({0,0.35f,0},{0.6f,0.25f,0.6f});
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
    } else if (group==GroupKind::Series) {
        // A shared floor, roof frame and supports join the actual member modules.
        // Opening the group zooms into these same units; it never scatters them.
        box(diorama_focus+Vec3{0,1.6f,0},{15.6f,0.06f,15.6f});
        box({-7.8f,0.8f,-10.3f},{0.08f,1.6f,0.08f});
        box({7.8f,0.8f,-10.3f},{0.08f,1.6f,0.08f});
    }
}
}
void detail_levels(const Hierarchy& hierarchy,const Camera& camera,std::vector<unsigned char>& result,std::span<const unsigned char> previous) {
    if (!previous.empty()) result.assign(previous.begin(),previous.end());
    result.resize(hierarchy.size(),0);
    const auto eye=camera.eye();
    for (auto id:hierarchy.visible_nodes()) {
        const auto offset=eye-hierarchy.focus(id);
        const auto ratio=hierarchy.node(id).transform.scale/std::max(0.0000001f,std::sqrt(dot(offset,offset)));
        result[id]=disclosure_level(ratio,result[id]);
    }
}
Presentation present(const Hierarchy& hierarchy,NodeId current,std::size_t selected,
    std::span<const unsigned char> levels,const AppearanceMap& appearance,const PresentationConfig& config,NodeId excluded) {
    Presentation result;
    std::vector<NodeId> ancestors;
    for (auto id=current;id!=no_node;id=hierarchy.node(id).parent) ancestors.push_back(id);
    const auto parent=hierarchy.node(current).parent;
    const auto visible_root=parent==no_node ? current:parent;
    enclosure(result.boxes,hierarchy.node(visible_root).transform,hierarchy.node(visible_root).group,semantic_color(Category::Directory)*0.6f);
    const auto visit=[&](auto&& self,NodeId id,unsigned depth)->void {
        if (id==current) depth=0;
        const auto& node=hierarchy.node(id);
        const auto& layout=node.world;
        const DisclosureConfig disclosure;
        // The active page and its immediate child dioramas always retain their
        // complete bounded page, including groups and remainder summaries.
        const bool immediate_child=node.parent==current;
        const std::size_t limit=(id==current || immediate_child || levels[id]==2) ? layout.objects.size():
            levels[id]==1 ? disclosure.nearby_objects:disclosure.distant_objects;
        for (std::size_t i=0;i<layout.objects.size();++i) {
            const auto child=hierarchy.child(id,i);
            if (excluded!=no_node && child==excluded) continue; // Supplied by another composed scene.
            const bool active=id==current && i==selected;
            const bool on_path=std::find(ancestors.begin(),ancestors.end(),child)!=ancestors.end();
            const auto& object=layout.objects[i];
            ObjectAppearance style;
            if (const auto found=appearance.find(id);found!=appearance.end() && i<found->second.size()) style=found->second[i];
            const auto color=semantic_color(object.item.category)*((0.5f+0.5f*object.item.salience)*style.brightness);
            const bool draw=layout.detail_rank[i]<limit || active || on_path || style.force_detail;
            if (id==current) {
                result.targets.push_back({object.center,object.size,color});
                result.interactive.push_back(style.selectable); result.drawn.push_back(draw);
            }
            if (!draw) continue;
            const bool container=object.item.category==Category::Directory || object.item.group!=GroupKind::None;
            if (container) {
                const auto transform=compose(node.transform,contained_transform(node.local.objects[i]));
                enclosure(result.boxes,transform,object.item.group,color);
                const bool near_context=std::find(ancestors.begin(),ancestors.end(),id)!=ancestors.end();
                if (!style.contents_status.empty() && (child==no_node || hierarchy.node(child).world.objects.empty()))
                    result.boxes.push_back({transform.point(diorama_focus+Vec3{0,0.25f,0}),Vec3{0.7f,0.5f,0.7f}*transform.scale,color});
                if (child!=no_node && (id==current || on_path || (depth<config.nested_depth && (near_context || levels[id]>0 || depth<2))))
                    self(self,child,depth+1);
            } else if (levels[id]==0 && !active && !style.force_detail)
                result.boxes.push_back({object.center,object.size*0.65f,color});
            else file_geometry(result.boxes,file_shape(object.item.category),object.center,object.size,color);
            if (active)
                result.boxes.push_back({object.center,object.size+Vec3{0.12f,0.12f,0.12f}*node.transform.scale,selection_color});
            // Only the current browsing level has names. All child/sibling
            // contents retain geometry, even when search forces full detail.
            if (id!=current) continue;
            std::string label=display_name(object.item.label)+style.contents_status;
            if (object.item.group!=GroupKind::None) label+=" ("+std::to_string(object.item.members.size())+")";
            result.labels.push_back({object.center+Vec3{0,object.size.y/2+0.2f*node.transform.scale,0},
                active ? selection_color:color,std::move(label),true,active,object.size.x,id==current});
        }
    };
    visit(visit,visible_root,0);
    return result;
}
}
