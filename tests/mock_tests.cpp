#include "app/browser.hpp"
#include "graphics/picking.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <set>
#include <stdexcept>

using namespace spatial;
namespace {
void check(bool condition,const char* message) {
    if (!condition) { std::cerr<<message<<'\n'; std::exit(1); }
}
bool near(float a,float b) { return std::abs(a-b)<0.0001f; }
EntryId child(const MockFilesystem& fs,EntryId parent,const std::string& name) {
    for (auto id:fs.children(parent)) if (fs.entry(id).name==name) return id;
    throw std::runtime_error("Missing fixture: "+name);
}
std::size_t groups(const std::vector<Item>& items,GroupKind kind) {
    return static_cast<std::size_t>(std::count_if(items.begin(),items.end(),[&](const Item& i) { return i.group==kind; }));
}
void coverage(const std::vector<Item>& items,const std::vector<EntryId>& ids) {
    std::vector<EntryId> actual;
    for (const auto& item:items) actual.insert(actual.end(),item.members.begin(),item.members.end());
    auto expected=ids;
    std::sort(actual.begin(),actual.end()); std::sort(expected.begin(),expected.end());
    check(actual==expected,"Grouping preserves every entry exactly once");
}
void test_salience() {
    Entry recent{1,0,"notes.txt",EntryKind::File,mock_now,false};
    auto older=recent; older.modified-=86400;
    auto oldest=recent; oldest.modified-=86400*365;
    check(salience(recent,mock_now)>salience(older,mock_now) && salience(older,mock_now)>salience(oldest,mock_now),
        "Recency continuously increases salience");
    auto half=recent; half.modified-=7*86400;
    check(near(salience_breakdown(half,mock_now).recency_bonus,0.12f),"Recency half-life is deterministic");
    auto future=recent; future.modified+=100;
    check(near(salience(future,mock_now),salience(recent,mock_now)),"Future timestamps are clamped");
    auto generated=recent; generated.name="main.o";
    auto binary=recent; binary.name="app.bin";
    auto executable=recent; executable.name="run.sh"; executable.executable=true;
    auto hidden=recent; hidden.name=".notes.txt";
    for (const auto& low:{generated,binary,executable,hidden})
        check(salience(low,mock_now)<salience(recent,mock_now) && salience(low,mock_now)>0,
            "Low-interest entries deweighted, not removed");
    check(classify(generated)==Category::Generated && classify(binary)==Category::Binary &&
        classify(executable)==Category::Executable,"Semantic classification");
    AnalysisConfig config; config.generated_weight=1;
    check(salience(generated,mock_now,config)>salience(generated,mock_now),"Generated weight is replaceable");
    config.recency_half_life=0;
    check(std::isfinite(salience(older,mock_now,config)),"Invalid half-life cannot divide by zero");
    auto dir=oldest; dir.kind=EntryKind::Directory;
    check(salience(dir,mock_now)>=0.6f,"Old directories remain navigable");
}
void test_series() {
    MockFilesystem fs;
    const auto add=[&](std::string name) { return fs.add(root_id,std::move(name),EntryKind::File,mock_now); };
    const auto ten=add("frame_010.png"), eight=add("frame_008.png"), nine=add("frame_009.png");
    add("frame_012.png"); add("frame_013.png"); add("frame_014.jpg");
    add("other_001.png"); add("frame_184467440737095516160.png");
    fs.add(root_id,"frame_011.png",EntryKind::Directory,mock_now);
    auto ids=fs.children(root_id);
    auto items=analyze(fs,ids,mock_now);
    check(groups(items,GroupKind::Series)==1,"Only consecutive runs with matching prefix/type form a series");
    const auto series=std::find_if(items.begin(),items.end(),[](const Item& i) { return i.group==GroupKind::Series; });
    check(series->members==std::vector<EntryId>{eight,nine,ten},"Series uses numeric order, independent of insertion");
    coverage(items,ids);
    const auto repeated=analyze(fs,ids,mock_now);
    check(repeated.size()==items.size(),"Deterministic series result");
    for (std::size_t i=0;i<items.size();++i) check(items[i].members==repeated[i].members,"Deterministic membership and order");
    MockFilesystem unpadded;
    for (const auto* name:{"img9.png","img10.png","img11.png"}) unpadded.add(0,name,EntryKind::File,mock_now);
    check(groups(analyze(unpadded,unpadded.children(0),mock_now),GroupKind::Series)==1,"Unpadded numeric suffixes");
    MockFilesystem ambiguous;
    for (const auto* name:{"img1.png","img01.png","img2.png","img3.png"}) ambiguous.add(0,name,EntryKind::File,mock_now);
    check(groups(analyze(ambiguous,ambiguous.children(0),mock_now),GroupKind::Series)==0,"Duplicate ordinals are ambiguous");
    MockFilesystem gaps;
    for (const auto* name:{"x1.png","x2.png","x3.png","x8.png","x9.png","x10.png"}) gaps.add(0,name,EntryKind::File,mock_now);
    check(groups(analyze(gaps,gaps.children(0),mock_now),GroupKind::Series)==2,"Gaps split independent runs");
}
void test_embedded_series() {
    MockFilesystem fs;
    std::vector<EntryId> expected;
    for (int i:{2,0,1}) expected.push_back(fs.add(0,"older-note-"+std::to_string(i)+"-draft.txt",EntryKind::File,mock_now));
    fs.add(0,"older-note-3-final.txt",EntryKind::File,mock_now);
    fs.add(0,"older-note-3-draft.png",EntryKind::File,mock_now);
    const auto items=analyze(fs,fs.children(0),mock_now);
    const auto series=std::find_if(items.begin(),items.end(),[](const auto& i) { return i.group==GroupKind::Series; });
    check(series!=items.end() && series->label=="older-note-*-draft.txt", "Embedded numeric token creates a structural series");
    check(series->members==std::vector<EntryId>{expected[1],expected[2],expected[0]},"Embedded tokens preserve numeric order");
    coverage(items,fs.children(0));
    MockFilesystem multi;
    for (const auto* name:{"v2-frame-009-final.png","v2-frame-010-final.png","v2-frame-011-final.png"})
        multi.add(0,name,EntryKind::File,mock_now);
    const auto multiple=analyze(multi,multi.children(0),mock_now);
    check(groups(multiple,GroupKind::Series)==1 && multiple.front().label=="v2-frame-*-final.png",
        "One varying token with another constant numeric token");
    coverage(multiple,multi.children(0));
    MockFilesystem overlapping;
    for (int a=1;a<=3;++a) for (int b=1;b<=3;++b)
        overlapping.add(0,"a"+std::to_string(a)+"-b"+std::to_string(b)+".txt",EntryKind::File,mock_now);
    const auto grid=analyze(overlapping,overlapping.children(0),mock_now);
    coverage(grid,overlapping.children(0));
    check(groups(grid,GroupKind::Series)==3,"Competing numeric-token patterns are resolved deterministically without duplicate membership");
}

void test_related_and_fixture(const MockFilesystem& fs) {
    const auto documents=child(fs,0,"documents");
    const auto ids=fs.children(documents);
    const auto items=analyze(fs,ids,mock_now);
    check(groups(items,GroupKind::Related)==1,"Timestamp/name-related session detected");
    const auto group=std::find_if(items.begin(),items.end(),[](const Item& i) { return i.group==GroupKind::Related; });
    check(group->members.size()==3,"Old same-prefix document excluded from recent session");
    coverage(items,ids);
    MockFilesystem times;
    // A chain spanning >window must not merge into one large session.
    times.add(0,"session_alpha.txt",EntryKind::File,mock_now);
    times.add(0,"session_beta.txt",EntryKind::File,mock_now+500);
    times.add(0,"session_gamma.txt",EntryKind::File,mock_now+1000);
    times.add(0,"unrelated.txt",EntryKind::File,mock_now);
    times.add(0,"session_image.png",EntryKind::File,mock_now);
    check(groups(analyze(times,times.children(0),mock_now),GroupKind::Related)==0,"No timestamp chaining, unrelated names or mixed types");
    const auto media=child(fs,0,"media"), renders=child(fs,media,"renders"), take=child(fs,renders,"take01");
    check(fs.path(take)=="/mock/media/renders/take01","Several nested directory layers");
    const auto frames=analyze(fs,fs.children(take),mock_now);
    check(frames.size()==1 && frames[0].group==GroupKind::Series && frames[0].members.size()==20,"Fixture's 20 frames form one cohesive series");
    check(analyze(fs,frames[0].members,mock_now,{},false).size()==20,"Expanded series exposes individual files");
    check(fs.children(child(fs,0,"empty-directory")).empty(),"Empty-directory fixture");
    check(fs.children(child(fs,0,"dense-240")).size()==240,"Dense fixture size");
    const auto project=child(fs,0,"projects");
    bool rejected=false;
    try { auto copy=fs; copy.add(project,"../escape",EntryKind::File,mock_now); }
    catch (const std::invalid_argument&) { rejected=true; }
    check(rejected,"Mock paths cannot escape the tree");
}
void test_layout(const MockFilesystem& fs) {
    const auto ids=fs.children(child(fs,0,"dense-240"));
    const auto items=analyze(fs,ids,mock_now);
    check(items.size()<240 && groups(items,GroupKind::Artifacts)==1,"Dense fixture summarizes low-interest build output without excluding members");
    const auto first=arrange(items);
    check(first.objects.size()==20 && first.pages==(items.size()+18)/19,"Summary fits in the bounded page");
    check(first.objects.front().item.salience>=first.objects[1].item.salience,"Salience drives placement order");
    check(near(first.objects.front().center.x,0) && first.objects.front().size.x>=first.objects[1].size.x,"High salience has central placement and greater scale");
    std::set<EntryId> all_pages;
    for (std::size_t page=0;page<first.pages;++page) {
        const auto layout=arrange(items,page);
        std::vector<EntryId> represented;
        for (const auto& placement:layout.objects) {
            check(std::isfinite(placement.center.x) && placement.size.x>0,"Finite layout");
            represented.insert(represented.end(),placement.item.members.begin(),placement.item.members.end());
            if (placement.item.group!=GroupKind::Summary)
                all_pages.insert(placement.item.members.begin(),placement.item.members.end());
        }
        auto expected=ids; std::sort(expected.begin(),expected.end()); std::sort(represented.begin(),represented.end());
        check(expected==represented,"Page and summary retain every entry exactly once");
        for (std::size_t i=0;i<layout.objects.size();++i) for (std::size_t j=i+1;j<layout.objects.size();++j) {
            const auto& a=layout.objects[i]; const auto& b=layout.objects[j];
            check(std::abs(a.center.x-b.center.x)>=(a.size.x+b.size.x)/2 ||
                std::abs(a.center.z-b.center.z)>=(a.size.z+b.size.z)/2,"Placement bounds do not overlap");
        }
    }
    check(all_pages.size()==ids.size(),"Paging makes all entries individually reachable");
    check(arrange(items,999).page==first.pages-1,"Page index clamped");
    check(arrange({}).objects.empty(),"Empty layout");
    LayoutConfig config; config.page_size=3; config.columns=2;
    check(arrange(items,0,{},config).objects.size()==3,"Density is configurable");
    const auto shifted=arrange(items,0,{5,0,-2});
    check(near(shifted.objects[0].center.x-first.objects[0].center.x,5),"Layout origin supports spatial transitions");
    const auto directory=semantic_color(Category::Directory), image=semantic_color(Category::Image), generated=semantic_color(Category::Generated);
    check(directory.z>directory.x && image.x>image.y && generated.z<directory.z,"Terminal-inspired semantic palette");
}
std::size_t object_named(const Layout& layout,const std::string& name) {
    for (std::size_t i=0;i<layout.objects.size();++i) if (layout.objects[i].item.label==name) return i;
    throw std::runtime_error("Missing layout object: "+name);
}
void test_shapes() {
    check(file_shape(Category::Source)==FileShape::CodeSlab && file_shape(Category::Text)==FileShape::Document,"Source and text have distinct plate geometry");
    check(file_shape(Category::Image)==FileShape::ImagePanel,"Images use wide panels");
    check(file_shape(Category::Media)==FileShape::MediaPanels,"Media use paired panels");
    check(file_shape(Category::Archive)==FileShape::ArchiveStack,"Archives use stacked bands");
    check(file_shape(Category::Binary)==FileShape::Prism && file_shape(Category::Executable)==FileShape::ExecutableFrame,"Executable frame differs from binary prism");
    check(file_shape(Category::Generated)==FileShape::LowTile,"Generated output uses low tiles");
    check(file_shape(Category::Directory)==FileShape::Diorama,"Directories use dioramas");
}
void test_hierarchy() {
    const LocalTransform parent{{10,2,-3},0.5f}, child_transform{{2,0,4},0.2f};
    const Vec3 point{1,2,3};
    const auto a=compose(parent,child_transform).point(point), b=parent.point(child_transform.point(point));
    check(near(a.x,b.x) && near(a.y,b.y) && near(a.z,b.z),"Parent/child transform composition");
    Browser browser;
    const auto root=browser.hierarchy.node(0).world;
    const auto index=object_named(root,"projects");
    const auto projects=browser.hierarchy.child(0,index);
    check(projects!=no_node && browser.hierarchy.node(projects).transform.scale<1,"Parent already contains reduced child layout");
    const auto& project_layout=browser.hierarchy.node(projects).world;
    const auto engine=browser.hierarchy.child(projects,object_named(project_layout,"engine"));
    check(engine!=no_node && !browser.hierarchy.node(engine).world.objects.empty(),"Visible directory contains another populated nested directory");
    check(browser.scene.targets.size()==root.objects.size(),"Parent-level picking stays separate from miniature children");
    const auto source=browser.hierarchy.node(engine).world.objects.front().center;
    check(std::any_of(browser.scene.boxes.begin(),browser.scene.boxes.end(),[&](const SceneBox& box) {
        return near(box.center.x,source.x) && near(box.center.z,source.z);
    }),"Nested source geometry exists before entry without requiring distant labels");
    for (auto id:browser.hierarchy.visible_nodes()) {
        const auto& node=browser.hierarchy.node(id);
        for (std::size_t i=0;i<node.local.objects.size();++i) {
            const auto expected=node.transform.point(node.local.objects[i].center);
            const auto actual=node.world.objects[i].center;
            check(near(expected.x,actual.x) && near(expected.y,actual.y) && near(expected.z,actual.z),"World positions come from local transforms");
            check(near(node.world.objects[i].size.x,node.local.objects[i].size.x*node.transform.scale),"World dimensions inherit parent scale");
            const auto nested=browser.hierarchy.child(id,i);
            if (nested==no_node) continue;
            const auto& container=node.world.objects[i];
            for (const auto& inside:browser.hierarchy.node(nested).world.objects)
                check(std::abs(inside.center.x-container.center.x)+inside.size.x/2<=container.size.x/2+0.00001f &&
                    std::abs(inside.center.z-container.center.z)+inside.size.z/2<=container.size.z/2+0.00001f,
                    "Nested objects remain inside the parent's diorama footprint");
        }
    }
    const auto original=browser.hierarchy.node(projects).transform;
    browser.hierarchy.ensure_preview(projects,3);
    check(near(original.scale,browser.hierarchy.node(projects).transform.scale) &&
        near(original.translation.x,browser.hierarchy.node(projects).transform.translation.x),"Adding deeper LOD does not move existing layouts");
    browser.hierarchy.ensure_preview(0,100);
    for (auto id:browser.hierarchy.visible_nodes()) check(browser.hierarchy.node(id).depth<=browser.hierarchy.config.max_depth,"Nested detail has bounded depth");
    check(browser.hierarchy.visible_nodes().size()<150,"No unlimited full-detail hierarchy");
}
void test_continuity() {
    Browser browser;
    const auto index=object_named(browser.layout,"media");
    const auto media=browser.hierarchy.child(0,index);
    const auto existing=browser.hierarchy.node(media).world.objects.front();
    const auto original_transform=browser.hierarchy.node(media).transform;
    const auto before=browser.camera.state();
    browser.act({ActionKind::Open,index});
    check(browser.current_node()==media,"Enter selects the already existing node");
    check(near(browser.camera.state().distance,before.distance) && near(browser.camera.state().focus.x,before.focus.x) &&
        near(browser.camera.state().focus.z,before.focus.z),"Entering does not cut camera position");
    check(near(browser.layout.objects.front().center.x,existing.center.x) &&
        near(browser.layout.objects.front().center.z,existing.center.z),"Entering preserves previously visible contents' world positions");
    check(near(browser.hierarchy.node(media).transform.scale,original_transform.scale),"Entering does not enlarge or replace the diorama");
    browser.update(0.2f);
    const auto halfway=browser.camera.state().distance;
    check(halfway<before.distance && halfway>24*original_transform.scale,"Camera travels continuously toward child scale");
    browser.update(1);
    check(near(browser.camera.state().distance,24*original_transform.scale),"Camera settles at child browsing scale");
    const auto target=browser.hierarchy.focus(media);
    check(near(browser.camera.state().focus.x,target.x) && near(browser.camera.state().focus.z,target.z),"Camera arrives inside the existing child diorama");
    check(browser.scene.targets.size()==browser.layout.objects.size(),"Picking targets refer only to the active context");
    const auto root_floor=diorama_focus+Vec3{0,-0.05f,0};
    check(std::any_of(browser.scene.boxes.begin(),browser.scene.boxes.end(),[&](const SceneBox& box) {
        return near(box.center.x,root_floor.x) && near(box.center.y,root_floor.y) &&
            near(box.center.z,root_floor.z) && near(box.size.x,16);
    }),"Parent enclosure stays in the scene when the child is entered");
    check(browser.scene.boxes.size()>browser.layout.objects.size(),"Scene includes hierarchy and surrounding context, not only active-directory objects");
    const auto inside=browser.camera.state();
    browser.act({ActionKind::Up});
    check(near(browser.camera.state().distance,inside.distance),"Parent navigation starts without a camera cut");
    browser.update(0.2f);
    check(browser.camera.state().distance>inside.distance && browser.camera.state().distance<before.distance,"Parent navigation smoothly zooms outward");
    browser.update(1);
    check(browser.current_node()==0 && near(browser.camera.state().distance,before.distance),"Parent overview restored");
    check(near(browser.hierarchy.node(media).world.objects.front().center.z,existing.center.z),"Child remains in the same place after returning to parent");
    // A compound sequence exposes the same units at the same transforms.
    const auto group_index=object_named(browser.layout,"older-note-*-draft.txt");
    const auto group=browser.hierarchy.child(0,group_index);
    const auto member=browser.hierarchy.node(group).world.objects.front();
    browser.act({ActionKind::Open,group_index}); browser.update(1);
    check(browser.navigation.context().group && browser.layout.total_entries==26,"Embedded-token sequence opens to all 26 members");
    check(browser.layout.objects.front().item.members==member.item.members && near(browser.layout.objects.front().center.x,member.center.x),
        "Series member IDs and transforms persist through expansion");
    check(browser.layout.objects.front().item.label=="older-note-0-draft.txt","Series starts at numeric ordinal zero");
    browser.act({ActionKind::NextPage});
    check(browser.layout.objects.front().item.label=="older-note-19-draft.txt","Later sequence members remain individually reachable in numeric order");
}

void test_browser() {
    Browser browser;
    const auto open=[&](const std::string& name) {
        for (std::size_t i=0;i<browser.layout.objects.size();++i)
            if (browser.layout.objects[i].item.label==name) { browser.act({ActionKind::Open,i}); return; }
        throw std::runtime_error("Missing visible object: "+name);
    };
    const auto original_revision=browser.scene_revision;
    browser.act({ActionKind::Reset});
    browser.act({ActionKind::Focus,0});
    check(browser.scene_revision==original_revision,"Camera-only actions do not rebuild scene geometry");
    open("media"); open("renders"); open("take01");
    check(browser.navigation.path(browser.filesystem)=="/mock/media/renders/take01","Enter nested mock directories");
    check(browser.camera.moving(),"Entering directory requests animation");
    open("frame_*.png");
    check(browser.navigation.context().group && browser.layout.objects.size()==20,"Series expands to individual files");
    browser.act({ActionKind::Focus,19}); browser.camera.update(1);
    const auto target=browser.layout.objects[19].center;
    check(near(browser.camera.state().focus.x,target.x) && near(browser.camera.state().focus.z,target.z),"Individual file focusing reaches placement");
    const auto scale=browser.hierarchy.node(browser.current_node()).transform.scale;
    check(near(browser.camera.state().distance,13*scale),"Deep group member focusing uses nested camera scale");
    const auto projected=project(target,browser.camera.view_projection(1),800,800);
    check(projected && std::abs(projected->x-400)<1 && std::abs(projected->y-400)<1,
        "Deep nested member projects to the viewport center");
    const auto hit=pick(browser.scene.targets,browser.camera,400,400,800,800);
    check(hit && *hit==19,"Deep nested series member remains pickable");
    browser.act({ActionKind::Open,19});
    check(browser.navigation.context().group.has_value(),"Opening a file only focuses it; no host file operation");
    browser.act({ActionKind::Up});
    check(!browser.navigation.context().group && browser.layout.objects.size()==1,"Collapse restores series");
    browser.act({ActionKind::Up}); browser.act({ActionKind::Up}); browser.act({ActionKind::Up});
    check(browser.navigation.context().directory==root_id,"Parent returns to root");
    open("dense-240");
    browser.act({ActionKind::NextPage});
    check(browser.layout.page==1 && browser.navigation.selected()==0,"Paging resets selection");
    const auto cached_nodes=browser.hierarchy.size();
    browser.act({ActionKind::PreviousPage}); browser.act({ActionKind::NextPage});
    check(browser.hierarchy.size()==cached_nodes,"Repeated paging reuses cached nodes instead of growing the scene");
    const auto summary=browser.layout.objects.size()-1;
    const auto members=browser.layout.objects[summary].item.members;
    browser.act({ActionKind::Open,summary});
    check(browser.navigation.context().group->members==members,"Summary expansion retains all low-salience members");
    browser.act({ActionKind::Up});
    check(browser.layout.page==1 && browser.navigation.selected()==summary,"Parent restores page and selection");
    browser.animations=false;
    browser.act({ActionKind::Focus,0});
    check(!browser.camera.moving(),"Reduced animation mode");
    browser.act({ActionKind::Up}); open("empty-directory");
    check(browser.layout.objects.empty(),"Empty directory remains navigable");
    browser.act({ActionKind::Focus,0}); browser.act({ActionKind::Open,0});
    browser.act({ActionKind::Up});
    check(browser.navigation.context().directory==root_id,"Return from empty directory");
    open("dense-240");
    check(browser.layout.page==1 && browser.navigation.context().page==1,"Re-entering preserves the page already visible in the nested preview");
}
void test_picking() {
    Camera camera;
    const std::vector<SceneBox> boxes{{{0,0,0},{2,2,2},{1,1,1}},{{0,0,-5},{2,2,2},{1,1,1}}};
    const auto hit=pick(boxes,camera,400,300,800,600);
    check(hit && *hit==0,"Picking selects nearest box on the camera ray");
    check(!pick(boxes,camera,-1,0,800,600) && !pick(boxes,camera,0,0,0,600),"Invalid viewport coordinates miss safely");
    const auto point=project({0,0,0},camera.view_projection(800.0f/600),800,600);
    check(point && near(point->x,400) && near(point->y,300),"Labels project onto focal center");
    check(!project(camera.eye()+(camera.eye()-camera.state().focus),camera.view_projection(1),800,800),"Labels behind camera are rejected");
}
}
int main() {
    test_salience(); test_series(); test_embedded_series(); test_shapes(); test_hierarchy(); test_continuity();
    const auto fs=make_mock_filesystem();
    test_related_and_fixture(fs); test_layout(fs); test_browser(); test_picking();
    std::cout<<"Mock filesystem, salience, series, grouping, layout, navigation, presentation and picking tests passed\n";
}
