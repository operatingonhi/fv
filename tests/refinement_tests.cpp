#include "app/browser.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>

using namespace spatial;
namespace {
void check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
bool same(Vec3 a,Vec3 b) { return a.x==b.x && a.y==b.y && a.z==b.z; }
void paging() {
    std::vector<Item> items;
    for (EntryId i=1;i<=1000;++i) items.push_back({"entry "+std::to_string(i),Category::Text,GroupKind::None,0.5f,{i}});
    const auto first=arrange(items);
    check(first.objects.size()==20 && first.objects.back().item.group==GroupKind::Summary,"Summary reserves the last visible slot");
    check(first.objects.back().center.z==first.objects[first.objects.size()-2].center.z,"Summary shares the final row");
    std::set<EntryId> reached;
    for (std::size_t page=0;page<first.pages;++page) {
        const auto layout=arrange(items,page);
        std::set<EntryId> represented;
        for (const auto& object:layout.objects) {
            for (auto id:object.item.members) check(represented.insert(id).second,"No duplicate page/summary member");
            if (object.item.group!=GroupKind::Summary) reached.insert(object.item.members.front());
        }
        check(represented.size()==1000 && layout.objects.size()<=20,"Every page retains all entries within the footprint");
    }
    check(reached.size()==1000,"Every entry is individually reachable");
    LayoutConfig tiny; tiny.page_size=1;
    check(arrange(items,0,{},tiny).objects.size()==2,"Tiny configured budget still makes progress");
    check(arrange({}).objects.empty() && arrange(std::span<const Item>{items.data(),1}).objects.size()==1,"Empty and singleton directories");

    Browser browser;
    auto find=[&](const char* name) {
        for (std::size_t i=0;i<browser.layout.objects.size();++i) if (browser.layout.objects[i].item.label==name) return i;
        throw std::runtime_error("Fixture object missing");
    };
    browser.act({ActionKind::Open,find("dense-240")}); browser.update(1);
    const auto objects=browser.layout.objects;
    const auto pose=browser.camera.state();
    const auto pages=browser.layout.pages;
    for (std::size_t page=1;page<=pages;++page) browser.act({ActionKind::Next});
    check(browser.layout.page==0,"Tab wraps through every set of entries");
    browser.act({ActionKind::Previous});
    check(browser.layout.page==pages-1,"Shift Tab wraps backwards");
    browser.act({ActionKind::Next});
    for (std::size_t i=0;i<objects.size();++i) check(same(objects[i].center,browser.layout.objects[i].center),"Page round trip preserves positions");
    check(same(pose.focus,browser.camera.state().focus) && !browser.camera.moving(),"Tab preserves camera pose");
    const auto revision=browser.scene_revision, evaluations=browser.lod_evaluations;
    for (int i=0;i<1000;++i) browser.update(0.016f);
    check(browser.scene_revision==revision && browser.lod_evaluations==evaluations,"Idle frames do not rebuild presentation or GPU geometry");
}
void stable_loading() {
    Hierarchy hierarchy;
    std::vector<Item> items;
    for (EntryId i=1;i<=40;++i) items.push_back({"entry "+std::to_string(i),Category::Text,GroupKind::None,0.5f,{i}});
    hierarchy.initialize({{0,items}},items);
    // Remove an early item and add another. Survivors must stay pinned, including
    // after visiting a different page (the old implementation lost these anchors).
    items.erase(items.begin()); items.push_back({"new",Category::Text,GroupKind::None,0.5f,{41}});
    hierarchy.upsert_directory({0,items},items);
    const auto before=hierarchy.node(0).world.objects;
    hierarchy.set_page(0,1); hierarchy.set_page(0,0);
    const auto& after=hierarchy.node(0).world.objects;
    for (std::size_t i=0;i<before.size();++i) check(same(before[i].center,after[i].center),"Loaded preview anchors survive page round trips");
}
void stress() {
    const auto start=std::chrono::steady_clock::now();
    Filesystem fs("/synthetic");
    for (int i=0;i<10000;++i) fs.add(0,"file-"+std::to_string(i*3)+"-draft.txt",EntryKind::File,mock_now);
    for (int i=0;i<2000;++i) fs.add(0,"artifact-"+std::to_string(i*3)+".o",EntryKind::File,mock_now);
    for (int group=0;group<40;++group) for (int i=0;i<60;++i)
        fs.add(0,"batch"+std::to_string(group)+"-frame_"+std::to_string(i)+".png",EntryKind::File,mock_now);
    for (int i=0;i<250;++i) fs.add(0,"directory "+std::to_string(i),EntryKind::Directory,mock_now);
    EntryId parent=0;
    for (int i=0;i<64;++i) parent=fs.add(parent,"deep",EntryKind::Directory,mock_now);
    for (const auto& name:{std::string(240,'x')+".txt",std::string("line\nbreak\t###.txt"),std::string("-dash [brackets] %.txt"),std::string("bytes-\xff.txt")})
        fs.add(0,name,EntryKind::File,mock_now);
    std::vector<DirectoryItems> directories;
    std::vector<EntryId> all;
    for (EntryId id=0;id<fs.size();++id) {
        all.push_back(id);
        if (fs.entry(id).kind==EntryKind::Directory) directories.push_back({id,analyze(fs,fs.children(id),mock_now)});
    }
    auto individuals=analyze(fs,all,mock_now,{},false);
    Hierarchy hierarchy; hierarchy.initialize(std::move(directories),individuals);
    check(hierarchy.size()<500,"Huge directory and deep tree do not eagerly materialize an unbounded hierarchy");
    Camera camera; camera.focus(hierarchy.focus(0),false,24);
    std::vector<unsigned char> levels; detail_levels(hierarchy,camera,levels);
    const auto scene=present(hierarchy,0,no_selection,levels);
    std::vector<unsigned char> far(hierarchy.size(),0);
    const auto distant=present(hierarchy,0,no_selection,far);
    check(!distant.boxes.empty() && distant.boxes.size()<scene.boxes.size(),"Distant dioramas retain contents with reduced geometry");
    check(distant.labels.empty(),"Distant content emits no unreadable labels");
    const auto before=hierarchy.node(0).world.objects;
    hierarchy.upsert_directory({0,analyze(fs,fs.children(0),mock_now)},std::move(individuals));
    const auto& after=hierarchy.node(0).world.objects;
    for (std::size_t i=0;i<before.size();++i) check(same(before[i].center,after[i].center),"Large unchanged snapshot keeps stable anchors");
    for (const auto& box:scene.boxes) check(std::isfinite(box.center.x) && box.size.x>0,"Stress presentation is finite");
    const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    std::cout<<"Synthetic entries="<<fs.size()<<" nodes="<<hierarchy.size()<<" boxes="<<scene.boxes.size()
        <<" distant boxes="<<distant.boxes.size()<<" labels="<<scene.labels.size()<<" total ms="<<elapsed<<'\n';
}
void display_and_lod() {
    check(display_name("line\n\tend")=="line\\x0a\\x09end","Control bytes display without breaking object rows");
    Browser browser;
    const auto positions=browser.layout.objects;
    browser.presentation_config.label_limit=0; browser.configure_presentation();
    for (const auto& label:browser.scene.labels) check(label.selected,"Zero label density retains selection only");
    for (std::size_t i=0;i<positions.size();++i) check(same(positions[i].center,browser.layout.objects[i].center),"Display settings preserve layout");
    for (std::size_t i=0;i<browser.layout.objects.size();++i)
        if (browser.layout.objects[i].item.label=="projects") { browser.act({ActionKind::Open,i}); break; }
    browser.update(1);
    browser.presentation_config.nested_depth=1; browser.configure_presentation();
    check(browser.scene.targets.size()==browser.layout.objects.size(),"LOD depth settings cannot hide the active context");
}
void camera_boundaries() {
    Camera camera;
    camera.animation_duration=0; camera.focus({1,0,0});
    check(!camera.moving() && camera.state().focus.x==1,"Zero duration settles immediately");
    camera.animation_duration=0.2f; camera.focus({2,0,0}); camera.update(0.1f);
    const auto before=camera.state(); camera.focus({-1,0,0});
    check(same(before.focus,camera.state().focus),"Interrupted transition starts at displayed pose");
    camera.zoom(1); check(!camera.moving(),"Zoom cancels active transition");
    const auto nan=std::numeric_limits<float>::quiet_NaN();
    camera.orbit(nan,nan); camera.zoom(nan); camera.update(nan);
    for (float aspect:{0.0f,-1.0f,nan,0.01f,100.0f})
        for (float value:camera.view_projection(aspect)) check(std::isfinite(value),"Degenerate aspect/input remains finite");
}
}
int main() {
    try { paging(); stable_loading(); stress(); display_and_lod(); camera_boundaries(); }
    catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
