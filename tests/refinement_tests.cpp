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
    check(!distant.boxes.empty() && distant.boxes.size()<=scene.boxes.size(),"Distant dioramas preserve structure within the normal geometry budget");
    check(distant.labels.size()==hierarchy.node(0).world.objects.size(),"Distant views retain every current-page name");
    for (const auto& label:distant.labels) check(label.required,"Distant views only name the current page");
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
    browser.presentation_config.nested_depth=0; browser.configure_presentation();
    std::size_t required=0;
    for (const auto& label:browser.scene.labels) if (label.required) ++required;
    check(required==browser.layout.objects.size(),"Nested detail settings never suppress current-page names");
    check(browser.scene.context_name=="mock","Current directory has an explicit heading");
    browser.act({ActionKind::Search,0,"nothing matches this filename"});
    const auto named=std::count_if(browser.scene.labels.begin(),browser.scene.labels.end(),[](const auto& label) { return label.required; });
    check(static_cast<std::size_t>(named)==positions.size(),"Search dimming does not suppress current-page names");
    browser.act({ActionKind::Cancel});
    for (std::size_t i=0;i<positions.size();++i) check(same(positions[i].center,browser.layout.objects[i].center),"Display settings preserve layout");
    for (std::size_t i=0;i<browser.layout.objects.size();++i)
        if (browser.layout.objects[i].item.label=="projects") { browser.act({ActionKind::Open,i}); break; }
    browser.update(1);
    browser.presentation_config.nested_depth=1; browser.configure_presentation();
    check(browser.scene.targets.size()==browser.layout.objects.size(),"LOD depth settings cannot hide the active context");
    browser.act({ActionKind::Up});
    for (std::size_t i=0;i<browser.layout.objects.size();++i)
        if (browser.layout.objects[i].item.label=="empty-directory") { browser.act({ActionKind::Open,i}); break; }
    check(browser.layout.objects.empty() && browser.scene.context_name=="empty-directory","Even an empty current directory has its name displayed");
}
void immediate_contents() {
    Filesystem fs("/fixture");
    const auto empty=fs.add(0,"empty",EntryKind::Directory,mock_now);
    const auto full=fs.add(0,"full",EntryKind::Directory,mock_now);
    for (int i=0;i<30;++i) fs.add(full,"entry-"+std::to_string(i*3)+".txt",EntryKind::File,mock_now);
    fs.add(full,"subdirectory",EntryKind::Directory,mock_now);
    std::vector<DirectoryItems> directories;
    std::vector<EntryId> ids;
    for (EntryId id=0;id<fs.size();++id) {
        ids.push_back(id);
        if (fs.entry(id).kind==EntryKind::Directory) directories.push_back({id,analyze(fs,fs.children(id),mock_now)});
    }
    Hierarchy hierarchy; hierarchy.initialize(std::move(directories),analyze(fs,ids,mock_now,{},false));
    std::vector<unsigned char> far(hierarchy.size(),0);
    PresentationConfig config; config.nested_depth=0;
    const auto scene=present(hierarchy,0,no_selection,far,{},config);
    check(scene.labels.size()==2 && std::all_of(scene.drawn.begin(),scene.drawn.end(),[](auto value) { return value; }),
        "All current-page objects and names survive distant LOD");
    for (std::size_t i=0;i<hierarchy.node(0).world.objects.size();++i) {
        const auto child=hierarchy.child(0,i);
        const auto& layout=hierarchy.node(child).world;
        const auto entry=hierarchy.node(0).world.objects[i].item.members.front();
        if (entry==empty) { check(layout.objects.empty(),"Known empty directory stays empty"); continue; }
        std::set<EntryId> represented;
        for (const auto& object:layout.objects) {
            represented.insert(object.item.members.begin(),object.item.members.end());
            check(std::any_of(scene.boxes.begin(),scene.boxes.end(),[&](const auto& box) {
                return std::abs(box.center.x-object.center.x)<0.00001f && std::abs(box.center.z-object.center.z)<0.00001f;
            }),"Every immediate child placement is drawn, including its remainder and subdirectories");
        }
        check(represented.size()==31 && layout.objects.size()==20,"Complete contents remain represented within a bounded diorama");
    }
    const auto detailed=present(hierarchy,0,no_selection,std::vector<unsigned char>(hierarchy.size(),2),{},config);
    check(detailed.boxes.size()>scene.boxes.size(),"Immediate files simplify at distance without losing representation");
    AppearanceMap highlighted;
    for (auto id:hierarchy.visible_nodes())
        for (std::size_t i=0;i<hierarchy.node(id).world.objects.size();++i)
            highlighted[id].push_back({1,true,true});
    const auto labelled=present(hierarchy,0,no_selection,std::vector<unsigned char>(hierarchy.size(),2),highlighted,config);
    check(labelled.labels.size()==hierarchy.node(0).world.objects.size(),
        "Nested file, directory and group names stay hidden at full detail with search highlighting");
    check(std::any_of(labelled.labels.begin(),labelled.labels.end(),[](const auto& label) { return label.text=="full" && label.required; }),
        "Immediate subdirectory titles remain displayed");
    check(std::none_of(labelled.labels.begin(),labelled.labels.end(),[](const auto& label) { return label.text=="subdirectory"; }),
        "Subsubdirectory titles stay hidden even at full detail or with search highlighting");
    for (std::size_t i=0;i<hierarchy.node(0).world.objects.size();++i) {
        if (hierarchy.node(0).world.objects[i].item.members.front()!=full) continue;
        const auto entered=present(hierarchy,hierarchy.child(0,i),no_selection,
            std::vector<unsigned char>(hierarchy.size(),2),highlighted,config);
        check(entered.labels.size()==hierarchy.node(hierarchy.child(0,i)).world.objects.size(),
            "Entering a directory names only its current-page entries");
        check(std::any_of(entered.labels.begin(),entered.labels.end(),[](const auto& label) { return label.text=="subdirectory" && label.required; }),
            "Entering a directory promotes its immediate subdirectory titles");
    }
    AppearanceMap waiting;
    for (const auto& object:hierarchy.node(0).world.objects) {
        ObjectAppearance style;
        if (object.item.members.front()==empty) style.contents_status=" [loading]";
        waiting[0].push_back(style);
    }
    const auto pending=present(hierarchy,0,no_selection,far,waiting,config);
    check(pending.boxes.size()==scene.boxes.size()+1,"Unknown contents have a marker instead of appearing empty");
    check(std::any_of(pending.labels.begin(),pending.labels.end(),[](const auto& label) { return label.text=="empty [loading]"; }),
        "Unknown directory contents are explicitly labelled");
}
void parent_frame() {
    const std::vector<Item> root{{"sibling",Category::Directory,GroupKind::None,0.7f,{1}},
        {"current",Category::Directory,GroupKind::None,0.6f,{2}}};
    const std::vector<Item> contents{{"file.txt",Category::Text,GroupKind::None,0.5f,{3}}};
    const std::vector<Item> sibling{{"sibling-file.txt",Category::Text,GroupKind::None,0.5f,{5}},
        {"sibling-subdirectory",Category::Directory,GroupKind::None,0.6f,{4}}};
    Hierarchy hierarchy; hierarchy.initialize({{0,root},{1,sibling},{2,contents},{4,{}}},
        {{"file.txt",Category::Text,GroupKind::None,0.5f,{3}}});
    const auto active=hierarchy.child(0,1);
    const auto anchor=contained_transform(hierarchy.node(0).local.objects[1]);
    const float scale=1/anchor.scale;
    hierarchy.transform_world({anchor.translation*(-scale),scale});
    const auto& node=hierarchy.node(active);
    check(std::abs(node.transform.scale-1)<0.00001f && dot(node.transform.translation,node.transform.translation)<0.00001f,
        "Parent backdrop maps its active child onto the existing root's identity frame");
    check(std::abs(node.world.objects[0].center.x-node.local.objects[0].center.x)<0.00001f &&
        std::abs(node.world.objects[0].center.z-node.local.objects[0].center.z)<0.00001f,
        "Parent framing preserves the existing current-directory content coordinates");
    const auto browsing=present(hierarchy,active,no_selection,std::vector<unsigned char>(hierarchy.size(),2));
    check(browsing.labels.size()==1 && browsing.labels.front().text=="file.txt",
        "Only CWD contents are named; sibling files and directories stay unnamed");
    const auto sibling_node=hierarchy.child(0,0);
    for (const auto& object:hierarchy.node(sibling_node).world.objects)
        check(std::any_of(browsing.boxes.begin(),browsing.boxes.end(),[&](const auto& box) {
            return std::abs(box.center.x-object.center.x)<0.00001f && std::abs(box.center.z-object.center.z)<0.00001f;
        }),"Sibling contents remain drawn without their names");
    const auto scene=present(hierarchy,active,no_selection,std::vector<unsigned char>(hierarchy.size(),2),{},{},active);
    check(scene.targets.empty() && scene.labels.empty(),
        "Composed backdrop excludes the current branch supplied by the main scene");
    for (auto id:hierarchy.visible_nodes()) {
        const auto& transformed=hierarchy.node(id);
        for (std::size_t i=0;i<transformed.local.objects.size();++i) {
            const auto expected=transformed.transform.point(transformed.local.objects[i].center);
            const auto actual=transformed.world.objects[i].center;
            check(dot(actual-expected,actual-expected)<0.00001f,"Reframed hierarchy keeps local/world transforms consistent");
        }
    }
}
void cached_branch_copy() {
    Filesystem source("/cache");
    const auto directory=source.add(0,"branch",EntryKind::Directory,mock_now);
    for (int i=0;i<35;++i) source.add(directory,"file-"+std::to_string(i*3)+".txt",EntryKind::File,mock_now);
    const auto broken=source.add(directory,"broken-link",EntryKind::Symlink,mock_now);
    DirectoryListing listing; listing.directory=source.entry(directory);
    for (auto id:source.children(directory)) {
        auto entry=source.entry(id);
        if (id==broken) { entry.broken_link=true; entry.error="Missing target"; }
        listing.entries.push_back(std::move(entry));
    }
    source.apply(directory,std::move(listing)); source.mark_loading(directory);
    std::vector<EntryId> all;
    for (EntryId id=0;id<source.size();++id) all.push_back(id);
    Hierarchy original;
    original.initialize({{0,analyze(source,source.children(0),mock_now)},
        {directory,analyze(source,source.children(directory),mock_now)}},analyze(source,all,mock_now,{},false));
    const auto node=original.child(0,0); original.set_page(node,1);
    Filesystem copied("/relocated"); const auto mapping=source.copy_subtree(directory,copied,0);
    check(copied.children(0).size()==36 && copied.state(0).state==LoadState::Ready && !copied.state(0).loading,
        "Cached metadata, including completeness, copies without inheriting queued worker state");
    check(copied.entry(mapping.at(broken)).broken_link && copied.entry(mapping.at(broken)).error=="Missing target",
        "Cache copying preserves broken-link metadata and errors");
    for (auto id:copied.children(0)) check(copied.entry(id).parent==0 &&
        copied.entry(id).full_path=="/relocated/"+copied.entry(id).name,"Copied entries retain destination paths and parent IDs");
    Hierarchy hierarchy; hierarchy.initialize({{0,{}}},{});
    original.copy_branch(node,hierarchy,0,mapping);
    const auto& before=original.node(node).local;
    const auto after=hierarchy.node(0).world;
    check(after.page==1 && after.objects.size()==before.objects.size(),"Rebased branch preserves its active page and density");
    for (std::size_t i=0;i<before.objects.size();++i) {
        check(same(before.objects[i].center,after.objects[i].center) && same(before.objects[i].size,after.objects[i].size),
            "Rebased branch retains local anchors and dimensions");
        for (std::size_t j=0;j<before.objects[i].item.members.size();++j)
            check(after.objects[i].item.members[j]==mapping.at(before.objects[i].item.members[j]),"Compound and summary members remap to the copied filesystem");
    }
    hierarchy.set_page(0,0); hierarchy.set_page(0,1);
    for (std::size_t i=0;i<after.objects.size();++i) check(same(after.objects[i].center,hierarchy.node(0).world.objects[i].center),
        "Rebased page round trip preserves cached anchors");
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
    try { paging(); stable_loading(); stress(); display_and_lod(); immediate_contents(); parent_frame(); cached_branch_copy(); camera_boundaries(); }
    catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
