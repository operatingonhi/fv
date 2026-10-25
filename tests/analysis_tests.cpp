#include "app/browser.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>
#include <stdexcept>

using namespace spatial;
namespace {
constexpr std::int64_t now=1'800'000'000;
void check(bool condition,const char* message) { if (!condition) throw std::runtime_error(message); }
bool near(float a,float b) { return std::abs(a-b)<0.0001f; }
Entry file(std::string name,std::int64_t age=0,std::uintmax_t bytes=100) {
    Entry entry;
    entry.name=std::move(name); entry.kind=EntryKind::File; entry.modified=now-age;
    entry.bytes=bytes; entry.size_known=true; entry.full_path="/fixture/"+entry.name;
    return entry;
}
Filesystem cache(std::vector<Entry> entries) {
    Filesystem fs("/fixture");
    DirectoryListing listing; listing.entries=std::move(entries); fs.apply(0,std::move(listing));
    return fs;
}
std::vector<Item> analyzed(const Filesystem& fs) { return analyze(fs,fs.children(0),now); }
std::size_t count(const std::vector<Item>& items,GroupKind kind) {
    return static_cast<std::size_t>(std::count_if(items.begin(),items.end(),[&](const Item& i) { return i.group==kind; }));
}
void coverage(const std::vector<Item>& items,const std::vector<EntryId>& ids) {
    std::vector<EntryId> actual;
    for (const auto& item:items) actual.insert(actual.end(),item.members.begin(),item.members.end());
    auto expected=ids; std::sort(actual.begin(),actual.end()); std::sort(expected.begin(),expected.end());
    check(actual==expected,"Analysis preserves each entry exactly once");
}
void recency_and_ranking() {
    const auto source=file("main.cpp",365*86400);
    check(salience(source,now)>salience(file("fresh.o"),now),"Old source outranks freshly generated output");
    check(salience(source,now)>salience(file("app.bin"),now),"Modification time cannot overwhelm semantic interest");
    float previous=salience(file("notes.txt"),now);
    for (std::int64_t hours:{1,12,24,72,168,336,720,8760}) {
        const float score=salience(file("notes.txt",hours*3600),now);
        check(score<previous,"Continuous decay across hours, weeks and years"); previous=score;
    }
    auto fresh=file("notes.txt");
    auto half=fresh; half.modified-=7*86400;
    check(near(salience_breakdown(half,now).recency_bonus,salience_breakdown(fresh,now).recency_bonus/2),"Half-life applies only to bounded recency contribution");
    auto future=fresh; future.modified+=86400;
    check(near(salience(future,now),salience(fresh,now)),"Future timestamps do not inflate score");
    auto unknown=fresh; unknown.modified_known=false;
    check(salience_breakdown(unknown,now).recency_bonus==0,"Unknown timestamp does not become a recent timestamp");
    check(salience(file(".editorconfig"),now)>salience(file(".private.txt"),now),"Useful project configuration receives a restrained hidden penalty");
    std::vector<float> scores(100,0.15f); scores.front()=0.9f;
    check(group_salience(scores)<0.5f,"One salient member cannot dominate a large group");
    check(group_salience(std::vector<float>(1000,0.1f))<salience(source,now),"Group size does not inflate artifacts above useful source");
    check(group_salience({})>0,"Empty/invalid group scoring remains bounded");
    const auto fs=cache({file("recent.o"),source,file("notes.txt",3600)});
    const auto items=analyzed(fs);
    check(items.front().label=="notes.txt" && items.back().label=="recent.o","Salience ranking balances recency, semantic prior and artifact penalty");
    const auto dir=file("build"); auto directory=dir; directory.kind=EntryKind::Directory;
    check(salience(directory,now,{},SalienceContext{20,true,0})>salience(directory,now,{},SalienceContext{20,true,1}),
        "Generated density reduces directory prominence only with known complete contents");
    check(salience(directory,now,{},SalienceContext{20,false,1})==salience(directory,now,{},SalienceContext{20,false,0}),
        "Partial directory samples do not imply artifact-heavy contents");
    auto many=directory; many.name="ordinary";
    check(salience(many,now,{},SalienceContext{100000,true,0})<=1 && salience(many,now,{},SalienceContext{100000,true,0})>
        salience(many,now),"Directory child-count contribution is positive and capped");
}
void cached_directory_context() {
    Filesystem fs("/fixture");
    const auto build=fs.add(0,"build",EntryKind::Directory,now);
    const auto source=fs.add(0,"source",EntryKind::Directory,now);
    DirectoryListing generated;
    for (const auto* name:{"main.o","camera.o","libengine.a","main.cpp.d"}) generated.entries.push_back(file(name));
    generated.complete=false; fs.apply(build,generated);
    DirectoryListing sources;
    for (const auto* name:{"main.cpp","camera.cpp","app.hpp","types.hpp"}) sources.entries.push_back(file(name));
    fs.apply(source,std::move(sources));
    auto before=analyzed(fs);
    const auto score=[&](const std::vector<Item>& items,EntryId id) {
        for (const auto& item:items) if (item.members==std::vector<EntryId>{id}) return item.salience;
        throw std::runtime_error("Missing directory score");
    };
    const float preview=score(before,build);
    Hierarchy hierarchy;
    const auto individuals=analyze(fs,fs.children(0),now,{},false);
    hierarchy.initialize({DirectoryItems{0,before}},individuals);
    const auto original=hierarchy.node(0).world.objects;
    generated.complete=true; fs.apply(build,std::move(generated));
    const auto after=analyzed(fs);
    check(score(after,build)<preview && score(after,build)<score(after,source),"Cached complete artifact density affects parent directory ranking");
    hierarchy.upsert_directory({0,after},individuals);
    for (const auto& old:original) {
        const auto& placements=hierarchy.node(0).world.objects;
        const auto found=std::find_if(placements.begin(),placements.end(),[&](const Placement& p) { return p.item.members==old.item.members; });
        check(found!=placements.end() && found->center.x==old.center.x && found->center.z==old.center.z,
            "Directory salience changes do not relocate existing dioramas");
    }
}

void artifact_classification() {
    for (const auto* name:{"main.o","main.cpp.d","main.cpp.obj","libtest.so.2.1","libtest.a","thing.dll",
        "code.pyc","CMakeCache.txt","notes.txt~",".notes.swp","bundle.min.js","bundle.js.map","schema.pb.h","api.generated.cpp"}) {
        const auto entry=file(name);
        check(artifact_kind(entry)!=ArtifactKind::None && classify(entry)==Category::Generated,"Explicit build/cache/dependency/temporary evidence recognized");
        check(salience(entry,now)>0 && salience(entry,now)<salience(file("main.cpp",365*86400),now),"Artifacts remain reachable but deweighted");
    }
    for (const auto* name:{"data.csv","analysis.log","world.map","mesh.obj","vector.d","molecule.pdb","symbols.lib","Makefile","build.cpp","libfoo.so.notes"})
        check(artifact_kind(file(name))==ArtifactKind::None,"Ordinary data, logs, meshes, D source and build scripts are not blanket artifacts");
    auto cache_directory=file("pkg"); cache_directory.kind=EntryKind::Directory;
    cache_directory.full_path="/fixture/node_modules/pkg";
    check(artifact_kind(cache_directory)==ArtifactKind::Cache && classify(cache_directory)==Category::Directory,
        "Known cache ancestry deweights directories without changing their diorama identity");
    auto dependency=file("main.d"); dependency.full_path="/fixture/build/main.d";
    check(artifact_kind(dependency)==ArtifactKind::Dependency,"Ambiguous dependency extension requires build context");
    auto symbols=file("main.lib"); symbols.full_path="/fixture/build/main.lib";
    check(artifact_kind(symbols)==ArtifactKind::Library,"Ambiguous library suffix needs build context");
    auto mesh=file("main.obj"); mesh.full_path="/fixture/build/main.obj";
    check(artifact_kind(mesh)==ArtifactKind::Object && classify(file("mesh.obj"))==Category::Media,"OBJ mesh and compiler output distinguished by context");
    auto cached=file("index.js"); cached.full_path="/fixture/node_modules/pkg/index.js";
    check(artifact_kind(cached)==ArtifactKind::Cache,"Known dependency/cache path components provide evidence without filesystem reads");
    auto link=file("main.o"); link.kind=EntryKind::Symlink;
    check(artifact_kind(link)==ArtifactKind::None && classify(link)==Category::Symlink,"Artifact suffix cannot override symlink semantics");
    auto dir=file("build"); dir.kind=EntryKind::Directory;
    check(artifact_kind(dir)==ArtifactKind::None && classify(dir)==Category::Directory,"Generic build directory name alone does not erase its semantic identity");
}
void sequences() {
    for (const auto& names:std::vector<std::vector<std::string>>{
        {"frame_009.png","frame_010.png","frame_011.png"},
        {"frame099.png","frame100.png","frame101.png"},
        {"older-note-0-draft.txt","older-note-1-draft.txt","older-note-2-draft.txt"},
        {"img9.png","img10.png","img11.png"}}) {
        std::vector<Entry> entries; for (const auto& name:names) entries.push_back(file(name));
        const auto fs=cache(entries); const auto items=analyzed(fs);
        check(items.size()==1 && items.front().group==GroupKind::Series,"Consistent padded/unpadded and embedded numeric series");
        coverage(items,fs.children(0));
    }
    for (const auto& names:std::vector<std::vector<std::string>>{
        {"frame_01.png","frame_2.png","frame_003.png"},
        {"release-v1.zip","release-v2.zip","release-v3.zip"},
        {"lib.so.1.0","lib.so.2.0","lib.so.3.0"},
        {"abi1x.cpp","abi2x.cpp","abi3x.cpp"},
        {"image001.png","other002.png","data003.png"},
        {"item1.txt","item3.txt","item5.txt"},
        {"frame01.png","frame1.png","frame2.png","frame3.png"}}) {
        std::vector<Entry> entries; for (const auto& name:names) entries.push_back(file(name));
        const auto fs=cache(entries); const auto items=analyzed(fs);
        check(count(items,GroupKind::Series)==0 && count(items,GroupKind::Related)==0,"Version/identifier/padding/gap false positives stay separate");
        coverage(items,fs.children(0));
    }
    const auto salvage=cache({file("frame001.png"),file("frame002.png"),file("frame003.png"),file("frame4.png")});
    check(count(analyzed(salvage),GroupKind::Series)==1,"An inconsistent extra name does not destroy a coherent padded run");
    const auto outlier=cache({file("img1.png",0,10),file("img2.png",0,10),file("img3.png",0,10),file("img4.png",0,100000)});
    check(count(analyzed(outlier),GroupKind::Series)==1,"A size outlier splits rather than destroys a coherent run");
    auto fs=cache({file("frame1.png",0,10),file("frame2.png",0,10),file("frame3.png",0,100000)});
    check(count(analyzed(fs),GroupKind::Series)==0,"Extreme known size mismatch rejects a weak filename sequence");
    Filesystem siblings;
    const auto a=siblings.add(0,"a",EntryKind::Directory,now), b=siblings.add(0,"b",EntryKind::Directory,now);
    std::vector<EntryId> ids;
    ids.push_back(siblings.add(a,"img1.png",EntryKind::File,now));
    ids.push_back(siblings.add(a,"img2.png",EntryKind::File,now));
    ids.push_back(siblings.add(b,"img3.png",EntryKind::File,now));
    check(count(analyze(siblings,ids,now),GroupKind::Series)==0,"Series never crosses directory boundaries");
}
void related() {
    const auto fs=cache({file("session-notes.txt",10,100),file("session-outline.txt",20,150),file("session-summary.txt",30,200),
        file("session-ancient.txt",86400,100),file("unrelated.txt",15,100),file("session-photo.png",15,100)});
    const auto items=analyzed(fs);
    check(count(items,GroupKind::Related)==1,"Name family plus extension, size and timestamp evidence forms one related group");
    const auto group=std::find_if(items.begin(),items.end(),[](const Item& i) { return i.group==GroupKind::Related; });
    check(group->members.size()==3 && !group->explanation.empty(),"Temporal outlier and unrelated/mixed types excluded; reason is inspectable");
    coverage(items,fs.children(0));
    const auto leading=cache({file("export-old.csv",950),file("export-start.csv",450),
        file("export-middle.csv",50),file("export-end.csv",0)});
    const auto sessions=analyzed(leading);
    check(count(sessions,GroupKind::Related)==1 && sessions.front().members.size()==3,
        "A weak leading fragment does not prevent finding a later bounded work session");
    coverage(sessions,leading.children(0));
    const auto disparate=cache({file("session_a.txt",0,10),file("session_b.txt",0,100000),file("session_c.txt",0,10)});
    check(count(analyzed(disparate),GroupKind::Related)==0,"Size outlier cannot form a related group");
    auto unknown=file("session_c.txt"); unknown.modified_known=false;
    const auto no_time=cache({file("session_a.txt"),file("session_b.txt"),unknown});
    check(count(analyzed(no_time),GroupKind::Related)==0,"Unknown timestamps are not evidence of a shared work session");
    const auto extensions=cache({file("session_a.txt"),file("session_b.md"),file("session_c.csv")});
    check(count(analyzed(extensions),GroupKind::Related)==0,"Broad text category alone cannot merge different extensions");
}
void density_and_disclosure() {
    std::vector<Entry> entries;
    for (int i=0;i<300;++i) entries.push_back(file("unit-"+std::to_string(i*3)+"-cache.o"));
    entries.push_back(file("important.cpp",100*86400));
    const auto fs=cache(entries); const auto items=analyzed(fs);
    check(items.size()==2 && items.front().label=="important.cpp" && count(items,GroupKind::Artifacts)==1,"Hundreds of artifacts occupy one low-priority region, preserving important source");
    coverage(items,fs.children(0));
    const auto artifact=std::find_if(items.begin(),items.end(),[](const Item& i) { return i.group==GroupKind::Artifacts; });
    check(artifact->members.size()==300 && analyze(fs,artifact->members,now,{},false).size()==300,"Every artifact remains individually available when expanded");
    AnalysisConfig expanded; expanded.artifact_summary_minimum=1000;
    check(analyze(fs,fs.children(0),now,expanded).size()==301,"Artifact density threshold is isolated/configurable");
    std::vector<Item> stable;
    for (EntryId i=1;i<=20;++i) { Item item; item.label="old note "+std::to_string(i); item.salience=0.2f; item.members={i}; stable.push_back(item); }
    const auto before=arrange(stable);
    Item valuable; valuable.label="important.cpp"; valuable.category=Category::Source; valuable.salience=0.9f; valuable.members={21};
    stable.push_back(valuable);
    const auto after=arrange(stable);
    for (std::size_t i=0;i<19;++i) check(before.objects[i].center.x==after.objects[i].center.x && before.objects[i].center.z==after.objects[i].center.z,"Importance disclosure does not reshuffle existing layout");
    const auto& summary=after.objects.back();
    check(summary.item.members.front()==21 && summary.item.members.size()==2 && summary.item.explanation.find("important.cpp")!=std::string::npos && after.detail_rank.back()==0,
        "High-salience omitted entry is discoverable through a prioritized, ranked remainder summary");
    auto page=arrange(stable,1);
    std::set<EntryId> all;
    for (const auto& object:page.objects) all.insert(object.item.members.begin(),object.item.members.end());
    check(all.size()==21 && page.objects.size()<=20,"Bounded page and summary retain all entries");
    check(disclosure_level(0.0021f,0)==0 && disclosure_level(0.0021f,1)==1,"Medium-distance hysteresis avoids flicker");
    check(disclosure_level(0.018f,1)==1 && disclosure_level(0.018f,2)==2,"Close-detail hysteresis avoids flicker");
    check(disclosure_level(0.1f,0)==2 && disclosure_level(0.0001f,2)==0,"Approaching reveals detail and retreat simplifies it");
    LayoutConfig numeric; numeric.rank_remainder=false;
    stable.back().salience=0.01f;
    const auto ordered=arrange(stable,1,{},numeric);
    check(ordered.objects.back().item.members.front()==1,"Series remainder can preserve original numeric order instead of score order");
}
}
int main() {
    try { recency_and_ranking(); cached_directory_context(); artifact_classification(); sequences(); related(); density_and_disclosure();
        std::cout<<"Milestone 4 salience, artifact, series, grouping, density and disclosure tests passed\n";
    } catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
