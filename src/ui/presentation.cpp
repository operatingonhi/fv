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
    case Category::Mixed: return {0.4f,0.8f,0.8f};
    }
    return {1,1,1};
}
Presentation present(const Layout& layout,std::size_t selected) {
    Presentation result;
    result.grid_origin=layout.origin;
    for (std::size_t i=0;i<layout.objects.size();++i) {
        const auto& object=layout.objects[i];
        const auto color=semantic_color(object.item.category)*(0.5f+0.5f*object.item.salience);
        const auto center=object.center,size=object.size;
        const float ground=center.y-size.y/2;
        result.targets.push_back({center,size,color});
        const auto box=[&](Vec3 c,Vec3 s) { result.boxes.push_back({c,s,color}); };
        if (object.item.group!=GroupKind::None) {
            // One enclosing platform makes the members a single cohesive object.
            box({center.x,ground+0.06f,center.z},{size.x,0.12f,size.z});
            const auto count=std::min<std::size_t>(6,object.item.members.size());
            for (std::size_t n=0;n<count;++n) {
                const float t=(static_cast<float>(n)+0.5f)/static_cast<float>(count);
                if (object.item.group==GroupKind::Series)
                    box({center.x-size.x*0.45f+size.x*0.9f*t,center.y,center.z},
                        {size.x*0.75f/static_cast<float>(count),size.y*0.65f,size.z*0.7f});
                else
                    box({center.x,ground+0.2f+(size.y-0.3f)*t,center.z},
                        {size.x*0.75f,0.06f,size.z*0.75f});
            }
        } else if (object.item.category==Category::Directory) {
            box({center.x,ground+0.08f,center.z},{size.x,0.16f,size.z});
            box({center.x,center.y,center.z-size.z*0.4f},{size.x,size.y,0.12f});
        } else box(center,size);
        if (i==selected)
            result.boxes.push_back({center,size+Vec3{0.16f,0.16f,0.16f},{1,0.95f,0.5f}});
        std::string label=object.item.label;
        if (object.item.group!=GroupKind::None) label+=" ("+std::to_string(object.item.members.size())+")";
        result.labels.push_back({center+Vec3{0,size.y/2+0.2f,0},i==selected ? Vec3{1,0.95f,0.5f}:color,
            std::move(label),i<7 || i==selected || object.item.group!=GroupKind::None,i==selected});
    }
    return result;
}
}
