#include "app/browser.hpp"

namespace spatial {
Browser::Browser() {
    std::vector<DirectoryItems> directories;
    std::vector<EntryId> all;
    for (EntryId id=0;id<filesystem.size();++id) {
        all.push_back(id);
        if (filesystem.entry(id).kind==EntryKind::Directory) {
            const auto children=filesystem.children(id);
            directories.push_back({id,analyze(filesystem,children,mock_now)});
        }
    }
    hierarchy.initialize(std::move(directories),analyze(filesystem,all,mock_now,{},false));
    camera.focus(hierarchy.focus(0),false,24);
    sync_view();
}
void Browser::refresh_scene() {
    detail_levels(hierarchy,camera,levels_);
    scene=present(hierarchy,current_,navigation.selected(),levels_);
    ++scene_revision;
}
void Browser::sync_view() {
    layout=hierarchy.node(current_).world;
    navigation.select(navigation.selected(),layout.objects.size());
    refresh_scene();
}
void Browser::focus_overview() {
    const auto scale=hierarchy.node(current_).transform.scale;
    camera.focus(hierarchy.focus(current_),animations,24*scale,scale);
}
void Browser::update(float seconds) {
    camera.update(seconds);
    detail_levels(hierarchy,camera,scratch_levels_);
    if (scratch_levels_!=levels_) refresh_scene();
}
void Browser::act(UiAction action) {
    switch (action.kind) {
    case ActionKind::None: return;
    case ActionKind::Focus:
    case ActionKind::Open: {
        if (action.index>=layout.objects.size()) return;
        const bool selection_changed=navigation.selected()!=action.index;
        navigation.select(action.index,layout.objects.size());
        const auto child=hierarchy.child(current_,action.index);
        if (action.kind==ActionKind::Open && child!=no_node &&
            navigation.open(filesystem,layout.objects[action.index].item)) {
            parents_.push_back(current_); current_=child;
            navigation.restore_page(hierarchy.node(current_).world.page);
            hierarchy.ensure_preview(current_,hierarchy.config.preview_depth);
            sync_view(); focus_overview();
        } else {
            const auto scale=hierarchy.node(current_).transform.scale;
            camera.focus(layout.objects[action.index].center,animations,13*scale,scale);
            if (selection_changed) refresh_scene();
        }
        break;
    }
    case ActionKind::Up:
        if (navigation.up()) {
            current_=parents_.back(); parents_.pop_back();
            sync_view(); focus_overview();
        }
        break;
    case ActionKind::PreviousPage:
    case ActionKind::NextPage:
        if ((action.kind==ActionKind::PreviousPage && layout.page==0) ||
            (action.kind==ActionKind::NextPage && layout.page+1>=layout.pages)) return;
        navigation.page(action.kind==ActionKind::PreviousPage ? -1:1,layout.pages);
        hierarchy.set_page(current_,navigation.context().page);
        sync_view(); focus_overview();
        break;
    case ActionKind::Reset: focus_overview(); break;
    }
}
}
