#include "app/browser.hpp"

namespace spatial {
Browser::Browser() {
    rebuild();
    camera.focus(origin_,false,24);
}
void Browser::rebuild() {
    const auto entries=navigation.entries(filesystem);
    const auto items=analyze(filesystem,entries,mock_now,{},!navigation.context().group.has_value());
    layout=arrange(items,navigation.context().page,origin_);
    navigation.select(navigation.selected(),layout.objects.size());
    scene=present(layout,navigation.selected());
    ++scene_revision;
}
void Browser::focus_overview() { camera.focus(origin_,animations,24); }
void Browser::act(UiAction action) {
    switch (action.kind) {
    case ActionKind::None: return;
    case ActionKind::Focus:
    case ActionKind::Open: {
        if (action.index>=layout.objects.size()) return;
        const bool selection_changed=navigation.selected()!=action.index;
        navigation.select(action.index,layout.objects.size());
        const auto& placement=layout.objects[action.index];
        if (action.kind==ActionKind::Open && navigation.open(filesystem,placement.item)) {
            parent_origins_.push_back(origin_);
            origin_=placement.center-Vec3{0,placement.size.y/2,0};
            rebuild();
            focus_overview();
        } else {
            camera.focus(placement.center,animations);
            if (selection_changed) {
                scene=present(layout,navigation.selected());
                ++scene_revision;
            }
        }
        break;
    }
    case ActionKind::Up:
        if (navigation.up()) {
            origin_=parent_origins_.back(); parent_origins_.pop_back();
            rebuild(); focus_overview();
        }
        break;
    case ActionKind::PreviousPage:
    case ActionKind::NextPage:
        if ((action.kind==ActionKind::PreviousPage && layout.page==0) ||
            (action.kind==ActionKind::NextPage && layout.page+1>=layout.pages)) return;
        navigation.page(action.kind==ActionKind::PreviousPage ? -1:1,layout.pages);
        rebuild(); focus_overview();
        break;
    case ActionKind::Reset: focus_overview(); break;
    }
}
}
