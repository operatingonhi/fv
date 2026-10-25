#include "app/browser.hpp"
#include <chrono>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace fs=std::filesystem;
using namespace spatial;
namespace {
void check(bool condition,const char* message) { if (!condition) throw std::runtime_error(message); }
struct TemporaryDirectory {
    fs::path path;
    TemporaryDirectory() {
        const auto base=fs::temp_directory_path();
        for (unsigned i=0;i<100;++i) {
            path=base/("spatial-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(i));
            std::error_code ec;
            if (fs::create_directory(path,ec)) return;
        }
        throw std::runtime_error("Cannot create temporary test directory");
    }
    ~TemporaryDirectory() {
        std::error_code ec;
        fs::permissions(path/"denied",fs::perms::owner_all,fs::perm_options::add,ec);
        fs::remove_all(path,ec);
    }
};
void write(const fs::path& path,const char* text="test data") {
    std::ofstream output(path); output<<text; check(static_cast<bool>(output),"Create temporary file");
}
const Entry& named(const Filesystem& model,EntryId parent,const std::string& name) {
    for (auto id:model.children(parent)) if (model.entry(id).name==name) return model.entry(id);
    throw std::runtime_error("Missing cached entry: "+name);
}
std::size_t object(const Browser& browser,const std::string& name) {
    for (std::size_t i=0;i<browser.layout.objects.size();++i) if (browser.layout.objects[i].item.label==name) return i;
    throw std::runtime_error("Missing spatial object: "+name);
}
void settle(Browser& browser) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    do {
        browser.update(0.016f);
        if (!browser.loading()) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while (std::chrono::steady_clock::now()<deadline);
    throw std::runtime_error("Background reads timed out");
}
void enumeration(const fs::path& root) {
    fs::create_directories(root/"child"/"grandchild");
    write(root/"child"/"notes.txt"); write(root/"child"/"grandchild"/"deep.cpp");
    write(root/"readme.txt","hello"); write(root/".hidden.txt"); write(root/"image.PNG");
    write(root/"app.bin"); write(root/"output.o"); write(root/"run.sh");
    fs::permissions(root/"run.sh",fs::perms::owner_exec,fs::perm_options::add);
    fs::last_write_time(root/"output.o",fs::file_time_type::clock::now());
    fs::last_write_time(root/"readme.txt",fs::file_time_type::clock::now()-std::chrono::hours(24*100));
    fs::create_symlink("readme.txt",root/"text-link");
    fs::create_directory_symlink("child",root/"directory-link");
    fs::create_symlink("absent",root/"broken-link");
    fs::create_symlink("loop",root/"loop");
    Filesystem model(root.string());
    const auto listing=read_directory(root);
    check(listing.error.empty() && listing.complete,"Complete one-directory enumeration");
    model.apply(0,listing);
    check(model.children(0).size()==11,"Enumerate regular files, directory, hidden files, and links");
    const auto file=named(model,0,"readme.txt");
    check(file.kind==EntryKind::File && file.size_known && file.bytes==5 && file.modified_known,"File size and timestamp metadata");
    check(file.permissions!=fs::perms::unknown && model.path(file.id)==(root/"readme.txt").string(),"Permissions and full path");
    check(named(model,0,".hidden.txt").hidden,"Hidden files included and flagged");
    check(named(model,0,"run.sh").executable && classify(named(model,0,"run.sh"))==Category::Executable,"Executable permission classification");
    check(classify(named(model,0,"image.PNG"))==Category::Image && classify(named(model,0,"output.o"))==Category::Generated &&
        classify(named(model,0,"app.bin"))==Category::Binary,"Real filenames retain semantic categories");
    for (const auto* name:{"text-link","directory-link","broken-link","loop"}) {
        const auto link=named(model,0,name);
        check(link.kind==EntryKind::Symlink && classify(link)==Category::Symlink && file_shape(classify(link))==FileShape::Linked,"Link semantics stay separate from targets");
    }
    check(named(model,0,"broken-link").broken_link && !named(model,0,"broken-link").error.empty(),"Broken symlink represented with error");
    check(named(model,0,"loop").broken_link,"Symlink cycle does not recurse");
    const auto explicit_link_root=read_directory(root/"directory-link");
    check(explicit_link_root.error.empty() && explicit_link_root.directory.kind==EntryKind::Directory && !explicit_link_root.entries.empty(),
        "An explicitly supplied directory symlink may be the starting location");
    check(named(model,0,"text-link").link_target=="readme.txt" && !named(model,0,"text-link").modified_known,"Explicit link target; no invented link timestamp");
    const auto child=named(model,0,"child").id;
    model.apply(child,read_directory(root/"child"));
    check(named(model,child,"notes.txt").parent==child && model.children(named(model,0,"directory-link").id).empty(),"Parent IDs and no automatic link traversal");
    const auto now=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const auto old_score=salience(named(model,0,"readme.txt"),now);
    auto recent=file; recent.modified=now;
    check(salience(recent,now)>old_score,"Real timestamps preserve continuous salience");
    auto generated=recent; generated.name="main.o";
    check(salience(generated,now)<salience(recent,now),"Generated deweighting preserved");
    const auto partial=read_directory(root,3);
    check(!partial.complete && partial.entries.size()==3,"Preview limit is explicit, not unbounded recursive loading");
}
void errors(const fs::path& root) {
    const auto missing=read_directory(root/"missing");
    check(!missing.error.empty(),"Missing directory is a recoverable result");
    check(!read_directory(root/"readme.txt").error.empty(),"Non-directory start is reported");
    write(root/"vanishing.txt"); fs::remove(root/"vanishing.txt");
    check(!read_metadata(root/"vanishing.txt").error.empty(),"Disappearing entry metadata is an error value");
    fs::create_directory(root/"denied");
    fs::permissions(root/"denied",fs::perms::none);
    const auto denied=read_directory(root/"denied");
    if (denied.error.empty()) std::cout<<"Permission-denied check skipped: process can read mode-000 directory\n";
    else check(denied.entries.empty(),"Permission denied reported without a crash");
    fs::permissions(root/"denied",fs::perms::owner_all);
    Browser failed(root/"missing"); settle(failed);
    check(failed.filesystem.state(0).state==LoadState::Error && !failed.status.empty(),"Unavailable start retains a status and usable application state");
    Filesystem cache(root.string());
    DirectoryListing preview; preview.complete=false;
    preview.entries.push_back(read_metadata(root/"readme.txt"));
    cache.apply(0,std::move(preview));
    DirectoryListing gone;
    cache.apply(0,std::move(gone));
    check(!named(cache,0,"readme.txt").error.empty(),"Entry removed between preview/full read remains explicitly marked");
}
void sequences_and_large_directory(const fs::path& root) {
    fs::create_directory(root/"sequences");
    fs::create_directory(root/"large");
    for (int i=1;i<=20;++i) {
        std::ostringstream name; name<<"frame_"<<std::setw(3)<<std::setfill('0')<<i<<".png";
        write(root/"sequences"/name.str());
    }
    for (int i=0;i<26;++i) write(root/"sequences"/("older-note-"+std::to_string(i)+"-draft.txt"));
    for (int i=0;i<600;++i) write(root/"large"/("item-"+std::to_string(i)+"-"+(i%2 ? "odd.txt":"even.txt")));
    Browser browser(root);
    settle(browser);
    const auto child=named(browser.filesystem,0,"child").id;
    const auto grandchild=named(browser.filesystem,child,"grandchild").id;
    check(browser.filesystem.state(child).state==LoadState::Ready && browser.filesystem.state(grandchild).state==LoadState::Unloaded,
        "Only immediate child contents are read; grandchild enumeration is deferred");
    const auto series=named(browser.filesystem,0,"sequences").id;
    const auto items=analyze(browser.filesystem,browser.filesystem.children(series),browser.now());
    std::size_t series_groups=0;
    for (const auto& item:items) if (item.group==GroupKind::Series) ++series_groups;
    check(series_groups==2,"Suffix and embedded sequences group from actual temporary files");
    const auto index=object(browser,"child");
    const auto node=browser.hierarchy.child(0,index);
    const auto before=browser.hierarchy.node(node).world.objects.front().center;
    const auto camera=browser.camera.state();
    browser.act({ActionKind::Open,index});
    check(browser.camera.state().distance==camera.distance,"Real directory entry does not cut the camera");
    settle(browser);
    check(browser.layout.objects.front().center.x==before.x && browser.layout.objects.front().center.z==before.z,"Loaded child retains preview transform");
    check(browser.filesystem.state(grandchild).state==LoadState::Ready,"Entering child progressively loads next level");
    browser.act({ActionKind::Up});
    const auto large=named(browser.filesystem,0,"large").id;
    check(browser.filesystem.state(large).state==LoadState::Preview && browser.filesystem.children(large).size()==64,"Large child preview is bounded");
    const auto large_node=browser.hierarchy.child(browser.current_node(),object(browser,"large"));
    const auto known=browser.hierarchy.node(large_node).world.objects;
    browser.act({ActionKind::Open,object(browser,"large")});
    settle(browser);
    for (const auto& old:known) if (old.item.group==GroupKind::None) {
        const auto found=std::find_if(browser.layout.objects.begin(),browser.layout.objects.end(),[&](const Placement& p) {
            return p.item.members==old.item.members;
        });
        check(found!=browser.layout.objects.end() && found->center.x==old.center.x && found->center.z==old.center.z,
            "Full loading does not move known preview objects");
    }
    for (std::size_t i=0;i<browser.layout.objects.size();++i) for (std::size_t j=i+1;j<browser.layout.objects.size();++j) {
        const auto& a=browser.layout.objects[i]; const auto& b=browser.layout.objects[j];
        check(std::abs(a.center.x-b.center.x)>=(a.size.x+b.size.x)/2 || std::abs(a.center.z-b.center.z)>=(a.size.z+b.size.z)/2,
            "Newly loaded entries occupy free slots without overlapping pinned previews");
    }
    check(browser.filesystem.state(large).state==LoadState::Ready && browser.filesystem.children(large).size()==600,"Active large directory fully loads off-thread");
    check(browser.layout.objects.size()<=21 && browser.layout.pages>=30,"Existing density bounds and pagination preserved");
    std::set<EntryId> reached;
    for (std::size_t page=0;page<browser.layout.pages;++page) {
        for (const auto& placement:browser.layout.objects) if (placement.item.group!=GroupKind::Summary)
            reached.insert(placement.item.members.begin(),placement.item.members.end());
        browser.act({ActionKind::NextPage});
    }
    check(reached.size()==600,"All large-directory entries remain reachable across pages");
    browser.act({ActionKind::Up});
    browser.act({ActionKind::Open,object(browser,"sequences")}); settle(browser);
    const auto group=object(browser,"frame_*.png");
    browser.act({ActionKind::Open,group});
    check(browser.layout.total_entries==20 && browser.layout.objects.size()==20,"Real series expands to individually addressable members");
    std::cout<<"Tested 600-file directory and two on-disk filename series\n";
}
void loading_regressions(const fs::path& root) {
    fs::create_directory(root/"gapped");
    for (int n:{1,2,3,8,9,10}) write(root/"gapped"/("frame"+std::to_string(n)+".png"));
    Browser browser(root/"gapped"); settle(browser);
    const auto original=browser.layout.objects;
    check(original.size()==2,"Disjoint runs share a pattern but form two compound structures");
    for (int n:{1,2,3}) fs::last_write_time(root/"gapped"/("frame"+std::to_string(n)+".png"),
        fs::file_time_type::clock::now()-std::chrono::hours(24*100));
    browser.filesystem.apply(0,read_directory(root/"gapped"));
    const auto& ids=browser.filesystem.children(0);
    browser.hierarchy.upsert_directory({0,analyze(browser.filesystem,ids,browser.now())},
        analyze(browser.filesystem,ids,browser.now(),{},false));
    const auto& updated=browser.hierarchy.node(0).world;
    for (const auto& old:original) {
        const auto found=std::find_if(updated.objects.begin(),updated.objects.end(),[&](const Placement& p) {
            return p.item.members==old.item.members;
        });
        check(found!=updated.objects.end() && found->center.x==old.center.x,"Same-pattern groups retain their own IDs and anchors when data updates");
    }
    Browser pending(root); settle(pending);
    pending.act({ActionKind::Open,object(pending,"large")});
    pending.act({ActionKind::Open,pending.layout.objects.size()-1});
    const auto active=pending.current_node();
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while (pending.status.find("Return to parent")==std::string::npos && std::chrono::steady_clock::now()<deadline) {
        pending.update(0.016f); std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    check(pending.status.find("Return to parent")!=std::string::npos && pending.current_node()==active,
        "Ready parent updates wait while a nested group is active");
    const auto& visible=pending.hierarchy.visible_nodes();
    check(std::find(visible.begin(),visible.end(),active)!=visible.end(),"Background regrouping cannot detach active context");
    pending.act({ActionKind::Up}); settle(pending);
    check(pending.layout.total_entries==600,"Returning applies the complete directory snapshot");
    std::stop_source cancelled; cancelled.request_stop();
    const auto scan=read_directory(root/"large",0,cancelled.get_token());
    check(!scan.complete && scan.entries.empty(),"Directory scan honors cooperative cancellation");
    {
        DirectoryLoader loader;
        loader.request(0,root/"large",0);
        loader.request(1,root/"sequences",0);
    } // Cancels pending work and joins without touching graphics or model lifetimes.
}

}
int main() {
    try {
        TemporaryDirectory temporary;
        enumeration(temporary.path); errors(temporary.path); sequences_and_large_directory(temporary.path); loading_regressions(temporary.path);
        std::cout<<"Real filesystem tests passed\n";
    } catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
