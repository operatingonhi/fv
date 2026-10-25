#include "app/browser.hpp"
#include "navigation/path.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace spatial;
namespace fs=std::filesystem;
namespace {
void check(bool condition,const char* message) { if (!condition) throw std::runtime_error(message); }
bool near(float a,float b) { return std::abs(a-b)<0.0001f; }
std::size_t object(const Browser& browser,std::string_view name) {
    for (std::size_t i=0;i<browser.layout.objects.size();++i) if (browser.layout.objects[i].item.label==name) return i;
    throw std::runtime_error("Missing browser object: "+std::string(name));
}
void settle(Browser& browser) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    do {
        browser.update(0.016f);
        if (!browser.loading()) { browser.update(1); return; }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while (std::chrono::steady_clock::now()<deadline);
    throw std::runtime_error("Browser worker timed out");
}
struct TemporaryDirectory {
    fs::path path;
    TemporaryDirectory() {
        const auto base=fs::temp_directory_path();
        for (int i=0;i<100;++i) {
            path=base/("spatial-browser-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(i));
            std::error_code ec; if (fs::create_directory(path,ec)) return;
        }
        throw std::runtime_error("Cannot create temporary browser fixture");
    }
    ~TemporaryDirectory() { std::error_code ec; fs::permissions(path/"denied",fs::perms::owner_all,fs::perm_options::add,ec); fs::remove_all(path,ec); }
};
void write(const fs::path& path) { std::ofstream output(path); output<<"test"; check(bool(output),"Write fixture"); }
void directional_selection() {
    const std::vector<SelectionPoint> points{{4,100,100},{8,150,100},{1,110,180},{9,60,100},{7,100,30}};
    check(directional_neighbor(points,4,Direction::Right)==8,"Right favors aligned visible neighbor over a sharp diagonal");
    check(directional_neighbor(points,4,Direction::Left)==9,"Left neighbor");
    check(directional_neighbor(points,4,Direction::Up)==7,"Screen-up neighbor");
    check(directional_neighbor(points,4,Direction::Down)==1,"Screen-down neighbor");
    check(!directional_neighbor({},4,Direction::Down),"Empty spatial selection");
    check(!directional_neighbor(std::vector<SelectionPoint>{{4,0,0}},4,Direction::Right),"No invented wrap across empty half-plane");
    check(directional_neighbor(points,std::nullopt,Direction::Right)==4,"Unselected context starts predictably");
    Browser browser;
    browser.viewport_width=1000; browser.viewport_height=700;
    browser.act({ActionKind::Select,0});
    const auto before=browser.camera.state();
    const auto selected=browser.navigation.selected();
    browser.act({ActionKind::Right});
    check(browser.navigation.selected()!=selected,"Projected keyboard movement changes selected object");
    check(near(browser.camera.state().focus.x,before.focus.x) && near(browser.camera.state().distance,before.distance),"Arrows select without changing camera pose");
    check(browser.navigation.context().selection && browser.navigation.context().selection->anchor==browser.selected_item()->members.front(),"Keyboard and inspector use the navigation selection key");
}
void queries_and_shared_selection() {
    Browser browser;
    const auto initial=browser.layout.objects;
    const auto source=object(browser,"navigator.cpp");
    browser.act({ActionKind::Focus,source});
    check(browser.navigation.context().selection->anchor==browser.selected_item()->members.front(),"Mouse/sidebar focus and metadata share identity");
    browser.act({ActionKind::Search,0,"NAVIGATOR.CPP"});
    check(browser.selected_item() && browser.selected_item()->label=="navigator.cpp","Case-insensitive search retains shared selection");
    check(browser.matching_objects()==1,"Search matches actual entry names");
    for (std::size_t i=0;i<initial.size();++i) check(initial[i].center.x==browser.layout.objects[i].center.x && initial[i].center.z==browser.layout.objects[i].center.z,"Search does not rebuild or reposition spatial layout");
    UiAction filter{ActionKind::Filter}; filter.filter.category=Category::Image;
    browser.act(filter);
    check(browser.navigation.selected()==no_selection && !browser.selected_item(),"No match produces one explicit empty selection");
    check(std::none_of(browser.scene.interactive.begin(),browser.scene.interactive.end(),[](auto x) { return x!=0; }),"Filtered objects cannot be picked accidentally");
    filter.filter=FilterSpec{}; browser.act(filter);
    browser.act({ActionKind::Search,0,"older-note-20-draft.txt"});
    check(browser.selected_item() && browser.selected_item()->group==GroupKind::Series,"Matching member emphasizes its compound series");
    browser.act({ActionKind::Open,browser.navigation.selected()});
    browser.act({ActionKind::NextMatch});
    check(browser.layout.page==1 && browser.selected_item()->label=="older-note-20-draft.txt","Search navigates to actual member across bounded pages");
    const auto member=browser.navigation.context().selection->anchor;
    browser.act({ActionKind::Back});
    check(!browser.navigation.context().group && browser.selected_item()->group==GroupKind::Series,"History restores enclosing spatial context and shared selection");
    browser.act({ActionKind::Forward});
    check(browser.layout.page==1 && browser.navigation.context().selection->anchor==member,"Forward restores page and exact selected member");
    browser.act({ActionKind::Cancel});
    check(browser.query().text.empty() && browser.navigation.context().selection->anchor==member,"Escape cancellation clears search without losing current member");
    browser.act({ActionKind::Back}); browser.act({ActionKind::Cancel});
    browser.act({ActionKind::Open,object(browser,"media")});
    check(!browser.history.can_forward(),"New navigation truncates forward history");
    check(name_matches("a file.txt","FILE") && !name_matches("abc","xyz") && name_matches("", ""),"Literal substring search including empty query");
    Filesystem cache("/not-a-real-directory");
    const auto directory=cache.add(0,"images",EntryKind::Directory,0);
    cache.add(directory,"needle.png",EntryKind::File,0);
    QuerySpec query; query.text="needle"; query.filter.category=Category::Image;
    const auto cached=match_cached(cache,cache.children(0),query);
    check(cached.eligible[directory] && cached.incomplete,"Cached nested match keeps its directory context; no filesystem lookup needed");
    QuerySpec hidden; hidden.filter.include_hidden=false;
    const auto secret=cache.add(0,".secret",EntryKind::Directory,0);
    cache.add(secret,"needle.png",EntryKind::File,0);
    check(!match_cached(cache,cache.children(0),hidden).eligible[secret],"Hidden filter does not resurrect excluded directories through children");
    Entry generated; generated.kind=EntryKind::File; generated.name="main.o";
    FilterSpec no_artifacts; no_artifacts.include_artifacts=false;
    check(!filter_matches(generated,no_artifacts),"Artifact filter uses independent analysis classification");
}
void paths_history_and_commands() {
    check(parse_location("~/a b","/base","/home/test").path=="/home/test/a b","Tilde expansion");
    check(parse_location("../a b","/base/child",{}).path=="/base/child/../a b","Relative path retains symlink-sensitive parent components for validation");
    check(parse_location("/absolute","/base",{}).path=="/absolute","Absolute path");
    check(!parse_location("~someone","/base","/home/test").error.empty(),"Unsupported user-home form is explicit");
    check(!parse_location("","/base",{}).error.empty() && !parse_location(std::string("bad\0name",8),"/base",{}).error.empty(),"Invalid path input is rejected");
    check(parse_location("$HOME","/base",{}).path=="/base/$HOME","Path entry never evaluates shell syntax");
    History history;
    history.push({"/a",1}); history.push({"/b",2}); history.move_to(0); history.push({"/c",3});
    check(!history.can_forward() && history.current().state==3 && history.can_back(),"History branching is deterministic");
    history.push({"/d",4}); history.invalidate(1);
    check(history.target(-1)==0,"Missing history entries can be skipped without leaving the current context");
    for (std::size_t i=4;i<100;++i) history.push({"/next",i});
    check(history.entries().size()==64,"History has a bounded entry count");
    const auto file=file_command("/tmp/a ; $(bad) ' quote.txt");
    check(file.arguments==std::vector<std::string>{"xdg-open","/tmp/a ; $(bad) ' quote.txt"},"File launch uses literal argument vectors, not a shell");
    check(file.directory=="/tmp" && !file_command("relative.txt").error.empty(),"File opener path and working directory construction");
    const auto terminal=terminal_command("/tmp/a b","my-terminal --title 'Spatial files' --flag=literal;$value");
    check(terminal.arguments==std::vector<std::string>{"my-terminal","--title","Spatial files","--flag=literal;$value"} && terminal.directory=="/tmp/a b","Terminal arguments retain quoting without shell expansion");
    check(!terminal_command("/tmp","broken 'quote").error.empty() && !terminal_command("relative","xterm").error.empty(),"Malformed terminal command/cwd fails gracefully");
    check(terminal_command("/tmp").arguments==std::vector<std::string>{"xdg-terminal-exec"},"Default terminal mechanism is isolated");
    DesktopLauncher launcher;
    check(!launcher.launch(LaunchPlan{{"/this-command-does-not-exist/spatial-test"},"/tmp",{}}).empty(),"Unavailable helper is reported without launching an application");
}
void real_navigation(const fs::path& root) {
    fs::create_directories(root/"a"/"child"); fs::create_directory(root/"b");
    write(root/"a"/"readme.txt"); write(root/"a"/"child"/"needle.txt"); write(root/"b"/"only-b.txt");
    fs::create_directory_symlink(root/"b",root/"link-b");
    const auto resolved=validate_path(root/"link-b"/"..",true);
    check(resolved.error.empty() && resolved.resolved==root,"Path canonicalization resolves symlinks before ..");
    check(!validate_path(root/"a"/"readme.txt",true).error.empty() && !validate_path(root/"missing",true).error.empty(),"Path validation checks directory type and missing locations");
    Browser browser(root/"a"); settle(browser);
    browser.camera.orbit(0.2f,0.1f);
    const auto pose=browser.camera.state();
    browser.act({ActionKind::Open,object(browser,"child")}); settle(browser);
    const auto child=browser.navigation.context().directory;
    browser.act({ActionKind::Back}); settle(browser);
    check(browser.navigation.context().directory==root_id && near(browser.camera.state().yaw,pose.yaw) && near(browser.camera.state().distance,pose.distance),"Back restores directory selection and useful camera pose");
    browser.act({ActionKind::Forward}); settle(browser);
    check(browser.navigation.context().directory==child,"Forward validates and restores child context");
    browser.act({ActionKind::Navigate,0,"../../b"}); settle(browser);
    check(browser.filesystem.path(0)==(root/"b").string() && object(browser,"only-b.txt")<browser.layout.objects.size(),"Relative path jump builds correct real-world context");
    browser.act({ActionKind::Back}); settle(browser);
    check(browser.filesystem.path(browser.navigation.context().directory)==(root/"a"/"child").string(),"History restores previous cached world and nested context");
    browser.act({ActionKind::Forward}); settle(browser);
    fs::remove_all(root/"a");
    const auto position=browser.history.position();
    browser.act({ActionKind::Back}); settle(browser);
    check(browser.history.position()==position && browser.filesystem.path(0)==(root/"b").string() && !browser.notice.empty(),"Missing history location reports an error without changing current context");
    browser.act({ActionKind::Navigate,0,(root/"absent").string()}); settle(browser);
    check(browser.filesystem.path(0)==(root/"b").string() && !browser.notice.empty(),"Failed path entry leaves current directory intact");
    browser.act({ActionKind::Navigate,0,(root/"link-b").string()}); browser.act({ActionKind::Cancel}); settle(browser);
    check(browser.filesystem.path(0)==(root/"b").string(),"Cancellation ignores obsolete path validation results");
    const auto same_count=browser.history.entries().size();
    browser.act({ActionKind::Navigate,0,"."}); settle(browser);
    check(browser.history.entries().size()==same_count,"Navigating to the current root does not create duplicate history");
    write(root/"b"/"gone.txt"); fs::create_symlink("gone.txt",root/"b"/"gone-link");
    // A fresh browser sees the file; removing it before activation must not spawn xdg-open.
    Browser disappearing(root/"b"); settle(disappearing);
    fs::remove(root/"b"/"gone.txt");
    disappearing.act({ActionKind::Open,object(disappearing,"gone.txt")}); settle(disappearing);
    check(!disappearing.notice.empty() && disappearing.notice.find("Resolve")!=std::string::npos,"Disappearing file activation is a recoverable validation error");
    disappearing.act({ActionKind::Open,object(disappearing,"gone-link")}); settle(disappearing);
    check(disappearing.notice.find("Resolve")!=std::string::npos,"Symlink target disappearance does not launch an application or crash");
    Browser racing(root/"b");
    racing.act({ActionKind::Navigate,0,root.string()}); settle(racing);
    check(racing.filesystem.path(0)==root.string(),"Stale scan generations cannot populate a different path's model");
    fs::create_directory(root/"denied"); fs::permissions(root/"denied",fs::perms::none);
    const auto denied=validate_path(root/"denied",true);
    if (denied.error.empty()) std::cout<<"Permission validation skipped for privileged process\n";
    fs::permissions(root/"denied",fs::perms::owner_all);
}
void paged_history(const fs::path& root) {
    const auto paged=root/"paged"; fs::create_directory(paged);
    for (int i=0;i<27;++i) fs::create_directory(paged/("dir-"+std::to_string(i)));
    Browser browser(paged); settle(browser);
    browser.act({ActionKind::NextPage}); settle(browser);
    const auto target=browser.layout.objects.front().item.members.front();
    browser.act({ActionKind::Open,0}); settle(browser);
    browser.act({ActionKind::Up}); settle(browser);
    browser.act({ActionKind::PreviousPage}); settle(browser);
    browser.act({ActionKind::Back}); settle(browser);
    check(browser.navigation.context().directory==target && browser.scene.targets.size()==browser.layout.objects.size(),"History restores ancestor pages so nested scene remains reachable");
    check(std::find(browser.hierarchy.visible_nodes().begin(),browser.hierarchy.visible_nodes().end(),browser.current_node())!=browser.hierarchy.visible_nodes().end(),"Restored history target is attached to the persistent world");
}
}
int main() {
    try {
        directional_selection(); queries_and_shared_selection(); paths_history_and_commands();
        TemporaryDirectory temporary; real_navigation(temporary.path); paged_history(temporary.path);
        std::cout<<"Milestone 5 selection, search/filter, path, history and command tests passed\n";
    } catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
