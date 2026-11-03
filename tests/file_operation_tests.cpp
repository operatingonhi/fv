#include "app/browser.hpp"
#include "filesystem/operations.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

using namespace spatial;
namespace fs=std::filesystem;
namespace {
void check(bool condition,const char* message) { if (!condition) throw std::runtime_error(message); }
struct Temporary {
    fs::path path;
    explicit Temporary(const fs::path& parent="/tmp") {
        auto pattern=(parent/"fv-file-test-XXXXXX").string();
        if (!::mkdtemp(pattern.data())) throw std::runtime_error("Cannot create disposable test directory");
        path=std::move(pattern);
    }
    ~Temporary() {
        std::error_code error;
        fs::permissions(path,fs::perms::owner_all,fs::perm_options::add,error);
        fs::recursive_directory_iterator it(path,error),end;
        while (!error && it!=end) {
            if (fs::is_directory(it->symlink_status(error))) fs::permissions(it->path(),fs::perms::owner_all,fs::perm_options::add,error);
            if (!error) it.increment(error);
        }
        fs::remove_all(path,error);
    }
};
void write(const fs::path& path,const std::string& text="contents") { std::ofstream output(path); output<<text; check(bool(output),"Fixture write succeeded"); }
std::string read(const fs::path& path) { std::ifstream input(path); return {std::istreambuf_iterator<char>(input),{}}; }
FileOperation operation(FileOperationKind kind,const fs::path& source,const fs::path& directory={}) {
    return {kind,source,directory,read_metadata(source).identity};
}
void settle(Browser& browser) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
    do {
        browser.update(1);
        check(std::chrono::steady_clock::now()<deadline,"Browser filesystem work completes");
        if (browser.loading()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while (browser.loading());
}
std::size_t object(const Browser& browser,const std::string& name) {
    for (std::size_t i=0;i<browser.layout.objects.size();++i) if (browser.layout.objects[i].item.label==name) return i;
    return no_selection;
}
void select(Browser& browser,const std::string& name) {
    const auto index=object(browser,name); check(index!=no_selection,"Expected fixture object is visible");
    browser.act({ActionKind::Select,index});
}
void filesystem_actions(const fs::path& root) {
    const auto from=root/"from", to=root/"to";
    fs::create_directories(from); fs::create_directory(to);
    const auto original=from/"-quoted ' file;$(unused).txt";
    write(original,"unchanged bytes");
    auto result=perform_file_operation(operation(FileOperationKind::Move,original,to));
    check(result.success && !fs::exists(original) && read(to/original.filename())=="unchanged bytes","Move handles arbitrary filenames without a shell");
    write(from/"conflict.txt","source"); write(to/"conflict.txt","destination");
    result=perform_file_operation(operation(FileOperationKind::Move,from/"conflict.txt",to));
    check(!result.success && read(from/"conflict.txt")=="source" && read(to/"conflict.txt")=="destination","Move refuses to overwrite an existing file");
    fs::create_symlink("absent",to/"broken-name"); write(from/"broken-name");
    result=perform_file_operation(operation(FileOperationKind::Move,from/"broken-name",to));
    check(!result.success && fs::is_symlink(fs::symlink_status(to/"broken-name")) && fs::exists(from/"broken-name"),"Broken destination symlinks count as occupied names");
    fs::create_directory(from/"tree"); fs::create_directory(to/"tree");
    write(from/"tree"/"inside.txt");
    result=perform_file_operation(operation(FileOperationKind::Move,from/"tree",to));
    check(!result.success && fs::exists(from/"tree"/"inside.txt") && fs::is_empty(to/"tree"),"Existing directories are never merged or replaced");
    result=perform_file_operation(operation(FileOperationKind::Move,from/"tree",from/"tree"));
    check(!result.success && fs::exists(from/"tree"/"inside.txt"),"Move refuses its own directory");
    fs::create_directory(from/"tree"/"nested");
    fs::create_directory_symlink(from/"tree"/"nested",root/"alias");
    result=perform_file_operation(operation(FileOperationKind::Move,from/"tree",root/"alias"/".."/"nested"));
    check(!result.success && fs::exists(from/"tree"/"inside.txt"),"Canonical destination detects a descendant reached through symlinks and dot-dot");
    result=perform_file_operation(operation(FileOperationKind::Move,from/"conflict.txt",from));
    check(!result.success && fs::exists(from/"conflict.txt"),"Moving into the same directory is a harmless error");
    result=perform_file_operation(operation(FileOperationKind::Move,from/"conflict.txt",root/"absent"));
    check(!result.success && fs::exists(from/"conflict.txt"),"Missing destinations retain the source");
    auto stale=operation(FileOperationKind::Delete,from/"conflict.txt");
    fs::rename(from/"conflict.txt",from/"held.txt"); write(from/"conflict.txt","replacement");
    result=perform_file_operation(stale);
    check(!result.success && read(from/"conflict.txt")=="replacement","Stale selection cannot delete a replacement entry");
    stale.kind=FileOperationKind::Move; stale.destination_directory=to;
    result=perform_file_operation(stale);
    check(!result.success && read(from/"conflict.txt")=="replacement","Stale cut cannot move a replacement entry");
    write(from/"edited.txt");
    const auto edited=operation(FileOperationKind::Delete,from/"edited.txt");
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    fs::last_write_time(from/"edited.txt",fs::file_time_type::clock::now()-std::chrono::hours(1));
    result=perform_file_operation(edited);
    check(!result.success && fs::exists(from/"edited.txt"),"Source changes after selection require a fresh confirmation even when its inode is unchanged");
    const auto gone=operation(FileOperationKind::Delete,from/"held.txt"); fs::remove(from/"held.txt");
    result=perform_file_operation(gone);
    check(!result.success && !result.error.empty(),"Disappearing sources report failure");
    auto invalid=operation(FileOperationKind::Delete,from); invalid.source="/";
    check(!perform_file_operation(invalid).success,"Filesystem roots cannot be deleted");
    std::stop_source stopped; stopped.request_stop();
    check(!perform_file_operation(operation(FileOperationKind::Delete,original.parent_path()/"conflict.txt"),stopped.get_token()).success && fs::exists(from/"conflict.txt"),"Cancelled operations do not start destructive work");
    write(root/"protected.txt","outside target");
    fs::create_symlink(root/"protected.txt",from/"link");
    result=perform_file_operation(operation(FileOperationKind::Move,from/"link",to));
    check(result.success && fs::is_symlink(fs::symlink_status(to/"link")) && read(root/"protected.txt")=="outside target","Move preserves a symlink rather than moving its target");
    result=perform_file_operation(operation(FileOperationKind::Delete,to/"link"));
    check(result.success && read(root/"protected.txt")=="outside target","Delete unlinks a symlink without deleting its target");
    fs::create_directory_symlink(to,from/"tree"/"outside");
    fs::create_symlink("missing",from/"tree"/"broken");
    result=perform_file_operation(operation(FileOperationKind::Delete,from/"tree"));
    check(result.success && !fs::exists(from/"tree") && read(to/"conflict.txt")=="destination","Recursive deletion leaves symlink targets untouched");
    fs::create_directory(from/"denied"); write(from/"denied"/"entry.txt");
    struct Restore { fs::path path; ~Restore() { std::error_code error; fs::permissions(path,fs::perms::owner_all,error); } } restore{from/"denied"};
    fs::permissions(from/"denied",fs::perms::none);
    const auto denied=operation(FileOperationKind::Delete,from/"denied");
    if (::geteuid()!=0) {
        result=perform_file_operation(denied);
        check(!result.success && !result.error.empty(),"Permission failures report deletion errors");
    }
    fs::permissions(from/"denied",fs::perms::owner_all);
    const auto fifo=from/"pipe"; check(::mkfifo(fifo.c_str(),0600)==0,"Create disposable special entry");
    check(!perform_file_operation(operation(FileOperationKind::Delete,fifo)).success && fs::exists(fifo),"Special entries are refused");
}
void cross_filesystem(const fs::path& root) {
    Temporary destination(fs::current_path());
    if (read_metadata(root).identity.device==read_metadata(destination.path).identity.device) {
        std::cout<<"Cross-filesystem test skipped: /tmp and test working directory share a filesystem\n"; return;
    }
    const auto source=root/"cross-tree";
    fs::create_directories(source/"nested"); write(source/"nested"/"data.txt","cross-device contents");
    fs::create_symlink("nested/data.txt",source/"relative-link");
    fs::create_symlink("absent",source/"broken-link");
    const auto timestamp=fs::file_time_type::clock::now()-std::chrono::hours(24*100);
    fs::last_write_time(source/"nested"/"data.txt",timestamp);
    auto result=perform_file_operation(operation(FileOperationKind::Move,source,destination.path));
    const auto target=destination.path/source.filename();
    check(result.success && !fs::exists(source) && read(target/"nested"/"data.txt")=="cross-device contents","Cross-filesystem directory move copies then removes the source");
    check(fs::read_symlink(target/"relative-link")=="nested/data.txt" && fs::is_symlink(fs::symlink_status(target/"broken-link")),"Cross-filesystem moves preserve relative and broken links");
    check(fs::last_write_time(target/"nested"/"data.txt")==timestamp,"Cross-filesystem moves preserve file modification times");
    write(root/"cross-file.txt","original"); write(destination.path/"cross-file.txt","existing");
    result=perform_file_operation(operation(FileOperationKind::Move,root/"cross-file.txt",destination.path));
    check(!result.success && read(root/"cross-file.txt")=="original" && read(destination.path/"cross-file.txt")=="existing","Cross-filesystem collisions preserve both files");
    fs::create_symlink("missing",root/"cross-link");
    result=perform_file_operation(operation(FileOperationKind::Move,root/"cross-link",destination.path));
    check(result.success && fs::read_symlink(destination.path/"cross-link")=="missing","A broken symlink can move across filesystems");
    fs::create_directory(destination.path/"copies");
    result=perform_file_operation(operation(FileOperationKind::Copy,root/"cross-file.txt",destination.path/"copies"));
    check(result.success && read(root/"cross-file.txt")=="original" && read(destination.path/"copies"/"cross-file.txt")=="original",
        "Cross-filesystem copy leaves the original intact");
    fs::create_directory(root/"readonly"); write(root/"readonly"/"data.txt");
    fs::permissions(root/"readonly",fs::perms::owner_read|fs::perms::owner_exec);
    result=perform_file_operation(operation(FileOperationKind::Move,root/"readonly",destination.path));
    const auto readonly=destination.path/"readonly";
    if (!fs::exists(readonly)) throw std::runtime_error("Read-only copy failed before publication: "+result.error);
    const bool permissions_preserved=(fs::status(readonly).permissions()&fs::perms::owner_write)==fs::perms::none;
    fs::permissions(readonly,fs::perms::owner_all);
    fs::permissions(root/"readonly",fs::perms::owner_all);
    check((result.success || (result.changed && !result.error.empty())) && read(readonly/"data.txt")=="contents","Read-only directories are copied completely before source removal is attempted");
    check(permissions_preserved,"Cross-filesystem moves preserve directory permissions");
    if (::geteuid()!=0) check(!result.success && fs::exists(root/"readonly"/"data.txt"),"Failed source removal retains its complete destination and reports failure");
    for (const auto& entry:fs::directory_iterator(destination.path))
        check(!entry.path().filename().string().starts_with(".fv-move-"),"Staging directories are cleaned after success and collision");
    std::cout<<"Cross-filesystem files, directories, symlinks, timestamps and collisions tested\n";
}
void browser_actions(const fs::path& root) {
    const auto source=root/"browser", target=root/"destination";
    fs::create_directory(source); fs::create_directory(target);
    write(source/"chosen.txt"); write(source/"keep.cpp"); write(source/"later.txt");
    Browser browser(source); settle(browser);
    select(browser,"chosen.txt");
    browser.act({ActionKind::DeleteFile});
    check(browser.file_prompt().has_value() && fs::exists(source/"chosen.txt"),"Delete only opens a confirmation");
    select(browser,"keep.cpp");
    check(browser.selected_item()->label=="chosen.txt","A pending confirmation freezes its selection");
    browser.act({ActionKind::CancelFileOperation});
    browser.act({ActionKind::ConfirmFileOperation}); settle(browser);
    check(fs::exists(source/"chosen.txt"),"Cancelled confirmation cannot perform a later delete");
    browser.act({ActionKind::Cut});
    check(browser.file_clipboard().has_value() && fs::exists(source/"chosen.txt"),"Cut does not change disk contents");
    browser.act({ActionKind::Navigate,0,target.string()}); settle(browser);
    browser.act({ActionKind::Paste});
    check(browser.file_busy(),"Paste submits work to the filesystem worker");
    browser.act({ActionKind::Back});
    check(fs::path(browser.filesystem.path(browser.navigation.context().directory))==target,"Navigation does not change the active operation's context");
    settle(browser);
    check(!browser.file_clipboard() && fs::exists(target/"chosen.txt") && !fs::exists(source/"chosen.txt") && object(browser,"chosen.txt")!=no_selection,"Successful paste clears cut and refreshes the destination");
    browser.act({ActionKind::Back}); settle(browser);
    check(object(browser,"chosen.txt")==no_selection && object(browser,"keep.cpp")!=no_selection,"History refreshes source worlds without ghost entries");
    select(browser,"keep.cpp");
    const auto anchor=browser.layout.objects[object(browser,"keep.cpp")].center;
    select(browser,"later.txt"); browser.act({ActionKind::MoveFile});
    browser.act({ActionKind::ConfirmFileOperation,0,""});
    check(browser.file_prompt().has_value() && !browser.file_busy() && fs::exists(source/"later.txt"),"Invalid destinations leave the confirmation available for correction");
    browser.act({ActionKind::ConfirmFileOperation,0,target.string()}); settle(browser);
    check(fs::exists(target/"later.txt") && object(browser,"later.txt")==no_selection,"Move dialog submits an explicit destination");
    const auto retained=browser.layout.objects[object(browser,"keep.cpp")].center;
    check(anchor.x==retained.x && anchor.z==retained.z,"Unchanged files retain their loaded anchors after a move");
    select(browser,"keep.cpp"); browser.act({ActionKind::Cut}); browser.act({ActionKind::Cancel});
    check(!browser.file_clipboard() && fs::exists(source/"keep.cpp"),"Escape clears the internal cut clipboard without changing files");
    browser.act({ActionKind::DeleteFile}); browser.act({ActionKind::ConfirmFileOperation}); settle(browser);
    check(!fs::exists(source/"keep.cpp") && browser.layout.objects.empty(),"Confirmed delete refreshes an empty source directory");
    Browser mock; mock.act({ActionKind::DeleteFile}); mock.act({ActionKind::Cut}); mock.act({ActionKind::Copy}); mock.act({ActionKind::Paste});
    check(!mock.file_prompt() && !mock.file_clipboard() && !mock.file_busy(),"Mocks never mutate real files");
}
void groups_and_retry(const fs::path& root) {
    const auto source=root/"groups", target=root/"occupied";
    fs::create_directory(source); fs::create_directory(target);
    for (const auto* name:{"frame001.png","frame002.png","frame003.png"}) write(source/name);
    write(target/"frame002.png","keep destination");
    Browser browser(source); settle(browser);
    check(browser.layout.objects.size()==1 && browser.selected_item()->group==GroupKind::Series,"Fixture creates a conservative series");
    browser.act({ActionKind::Cut});
    check(!browser.file_clipboard() && !browser.can_change_selection(),"Groups cannot be cut or deleted as synthetic files");
    browser.act({ActionKind::Open,0}); settle(browser); select(browser,"frame002.png");
    browser.act({ActionKind::Cut}); browser.act({ActionKind::Navigate,0,target.string()}); settle(browser);
    browser.act({ActionKind::Paste}); settle(browser);
    check(browser.file_clipboard().has_value() && fs::exists(source/"frame002.png") && read(target/"frame002.png")=="keep destination" && !browser.notice.empty(),"Failed paste retains cut and exposes the collision");
    fs::remove(target/"frame002.png"); browser.act({ActionKind::Paste}); settle(browser);
    check(!browser.file_clipboard() && !fs::exists(source/"frame002.png") && object(browser,"frame002.png")!=no_selection,"A failed paste can be retried after the collision is removed");
    browser.act({ActionKind::Back}); settle(browser);
    check(!browser.navigation.context().group && object(browser,"frame002.png")==no_selection,"Returning to a stale group falls back to its refreshed real directory");
    select(browser,"frame001.png"); browser.act({ActionKind::DeleteFile});
    const auto prompt=browser.file_prompt()->source;
    browser.act({ActionKind::Cancel});
    check(!browser.file_prompt() && fs::exists(prompt),"Escape cancels a delete confirmation");
}
void worker_queue(const fs::path& root) {
    const auto source=root/"queued.txt", target=root/"queued-destination";
    write(source); fs::create_directory(target);
    DirectoryLoader loader; loader.operate(operation(FileOperationKind::Move,source,target));
    loader.discard_reads_except(123);
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    for (;;) {
        const auto results=loader.poll_operations();
        if (!results.empty()) { check(results.front().success && fs::exists(target/"queued.txt"),"Navigation read cancellation cannot discard a queued file operation"); break; }
        check(std::chrono::steady_clock::now()<deadline,"Worker returns file-operation result");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
void nested_mutations(const fs::path& root) {
    auto directory=root/"deep-mutations"; fs::create_directory(directory);
    const auto start=directory;
    for (int i=0;i<5;++i) { directory/="next"; fs::create_directory(directory); write(directory/"note.txt"); }
    Browser browser(start); settle(browser);
    for (int i=0;i<5;++i) { browser.act({ActionKind::Open,object(browser,"next")}); settle(browser); }
    select(browser,"note.txt"); browser.act({ActionKind::DeleteFile}); browser.act({ActionKind::ConfirmFileOperation}); settle(browser);
    check(browser.layout.objects.empty(),"Delete at a rebased depth refreshes the leaf");
    browser.act({ActionKind::Up}); settle(browser);
    browser.act({ActionKind::Open,object(browser,"next")}); settle(browser);
    check(browser.layout.objects.empty(),"Cached parent return cannot resurrect deleted descendants");
    const auto grouped=root/"delete-group"; fs::create_directory(grouped);
    for (const auto* name:{"part001.png","part002.png","part003.png"}) write(grouped/name);
    Browser group(grouped); settle(group); group.act({ActionKind::Open,0}); settle(group);
    select(group,"part002.png"); group.act({ActionKind::DeleteFile}); group.act({ActionKind::ConfirmFileOperation}); settle(group);
    check(!group.navigation.context().group && object(group,"part002.png")==no_selection && fs::exists(grouped/"part001.png"),"Deleting a series member refreshes its real directory without detaching navigation");
}
void parent_child_moves(const fs::path& root) {
    const auto parent=root/"parent-child-moves";
    fs::create_directories(parent/"child");
    fs::create_directory(parent/"sibling");
    write(parent/"transfer.txt"); write(parent/"keep.cpp");
    fs::create_hard_link(parent/"transfer.txt",parent/"sibling"/"transfer.txt");
    Browser browser(parent); settle(browser);
    select(browser,"transfer.txt");
    const auto original=browser.layout.objects[browser.navigation.selected()];
    browser.act({ActionKind::Cut});
    browser.act({ActionKind::Open,object(browser,"child")}); settle(browser);
    const auto child=browser.current_node();
    const auto camera=browser.camera.state(); const auto grid=browser.scene.grid;
    browser.act({ActionKind::Paste}); settle(browser);
    check(!fs::exists(parent/"transfer.txt") && fs::exists(parent/"child"/"transfer.txt"),"Parent-to-child paste moves the real file");
    const auto& parent_objects=browser.hierarchy.node(browser.hierarchy.node(child).parent).world.objects;
    check(std::none_of(parent_objects.begin(),parent_objects.end(),[&](const auto& placement) {
        return placement.item.members==original.item.members;
    }),"Parent-to-child paste removes the old parent graphic while the child stays active");
    check(std::none_of(browser.scene.boxes.begin(),browser.scene.boxes.end(),[&](const auto& box) {
        return box.center==original.center;
    }),"The rendered scene contains no geometry at the old parent file position");
    check(browser.current_node()==child && browser.camera.state()==camera && browser.scene.grid==grid,
        "Removing the parent graphic preserves the active child, camera and grid");
    const auto parent_node=browser.hierarchy.node(child).parent;
    const auto& parent_layout=browser.hierarchy.node(parent_node).world.objects;
    const auto sibling=std::find_if(parent_layout.begin(),parent_layout.end(),[](const auto& object) { return object.item.label=="sibling"; });
    check(sibling!=parent_layout.end(),"Unrelated sibling remains in its parent");
    const auto sibling_node=browser.hierarchy.child(parent_node,static_cast<std::size_t>(sibling-parent_layout.begin()));
    check(browser.hierarchy.node(sibling_node).world.objects.size()==1 && fs::exists(parent/"sibling"/"transfer.txt"),
        "Removing a source leaves a same-named hard link in another directory visible");
    select(browser,"transfer.txt");
    const auto previous_child=browser.layout.objects[browser.navigation.selected()];
    browser.act({ActionKind::Cut}); browser.act({ActionKind::Up}); settle(browser);
    check(browser.filesystem.state(browser.navigation.context().directory).state==LoadState::Ready,
        "Returning to a directory invalidated by a file action reloads it");
    browser.act({ActionKind::Paste}); settle(browser);
    check(fs::exists(parent/"transfer.txt") && !fs::exists(parent/"child"/"transfer.txt"),"Child-to-parent paste moves the real file");
    const auto child_node=browser.hierarchy.child(browser.current_node(),object(browser,"child"));
    check(browser.hierarchy.node(child_node).world.objects.empty(),"Child-to-parent paste empties the old child preview");
    check(std::none_of(browser.scene.boxes.begin(),browser.scene.boxes.end(),[&](const auto& box) {
        return box.center==previous_child.center;
    }),"The rendered scene contains no geometry at the old child file position");
    browser.act({ActionKind::Back}); settle(browser);
    check(browser.layout.objects.empty(),"History does not restore the moved child's old graphic");
    browser.act({ActionKind::Forward}); settle(browser);
    check(object(browser,"transfer.txt")!=no_selection,"Forward history keeps the moved parent's new graphic");
}
void copies(const fs::path& root) {
    const auto source=root/"copy-source", target=root/"copy-target";
    fs::create_directories(source/"tree"/"nested"); fs::create_directory(target);
    write(source/"file.txt","copy contents"); write(source/"tree"/"nested"/"data.cpp");
    fs::create_symlink("nested/data.cpp",source/"tree"/"relative");
    fs::create_symlink("absent",source/"tree"/"broken");
    fs::create_directory_symlink(".",source/"tree"/"cycle");
    auto result=perform_file_operation(operation(FileOperationKind::Copy,source/"file.txt",target));
    check(result.success && read(source/"file.txt")=="copy contents" && read(target/"file.txt")=="copy contents",
        "Copy creates a new file while retaining the source");
    write(target/"file.txt","independent copy");
    check(read(source/"file.txt")=="copy contents","Modifying a copy does not alter the original");
    result=perform_file_operation(operation(FileOperationKind::Copy,source/"file.txt",target));
    check(!result.success && read(target/"file.txt")=="independent copy" && read(source/"file.txt")=="copy contents",
        "Copy refuses an existing name without altering either file");
    result=perform_file_operation(operation(FileOperationKind::Copy,source/"tree",target));
    check(result.success && fs::exists(source/"tree"/"nested"/"data.cpp") && read(target/"tree"/"nested"/"data.cpp")=="contents",
        "Recursive copy retains source contents");
    check(fs::read_symlink(target/"tree"/"relative")=="nested/data.cpp" && fs::read_symlink(target/"tree"/"broken")=="absent" &&
        fs::is_symlink(fs::symlink_status(target/"tree"/"cycle")),"Directory copies preserve relative, broken and cyclic links without following them");
    fs::create_directory(target/"readonly-copies");
    fs::permissions(source/"tree",fs::perms::owner_read|fs::perms::owner_exec);
    result=perform_file_operation(operation(FileOperationKind::Copy,source/"tree",target/"readonly-copies"));
    check(result.success && read(target/"readonly-copies"/"tree"/"nested"/"data.cpp")=="contents" &&
        (fs::status(target/"readonly-copies"/"tree").permissions()&fs::perms::owner_write)==fs::perms::none,
        "Read-only directories can be copied with their permissions retained");
    fs::permissions(source/"tree",fs::perms::owner_all);
    result=perform_file_operation(operation(FileOperationKind::Copy,source/"tree",source/"tree"/"nested"));
    check(!result.success && !fs::exists(source/"tree"/"nested"/"tree"),"Copy refuses directory-into-descendant recursion");
    result=perform_file_operation(operation(FileOperationKind::Copy,source/"tree"/"broken",target));
    check(result.success && fs::is_symlink(fs::symlink_status(source/"tree"/"broken")) && fs::is_symlink(fs::symlink_status(target/"broken")),
        "Copying a symlink copies the link itself");
    for (const auto& entry:fs::directory_iterator(target))
        check(!entry.path().filename().string().starts_with(".fv-move-"),"Copies leave no staging directories behind");
}
void copy_clipboard(const fs::path& root) {
    const auto parent=root/"copy-clipboard";
    fs::create_directories(parent/"first"); fs::create_directory(parent/"second");
    write(parent/"original.txt","clipboard source");
    Browser browser(parent); settle(browser); select(browser,"original.txt");
    const auto original=browser.layout.objects[browser.navigation.selected()];
    browser.act({ActionKind::Copy});
    check(browser.file_clipboard() && browser.file_clipboard()->kind==FileOperationKind::Copy && fs::exists(parent/"original.txt"),
        "Copy records a clipboard source without changing disk");
    browser.act({ActionKind::Open,object(browser,"first")}); settle(browser); browser.act({ActionKind::Paste}); settle(browser);
    check(browser.file_clipboard() && browser.file_clipboard()->kind==FileOperationKind::Copy && object(browser,"original.txt")!=no_selection &&
        read(parent/"original.txt")=="clipboard source" && read(parent/"first"/"original.txt")=="clipboard source",
        "Copy-paste preserves its source and keeps the clipboard for reuse");
    const auto parent_node=browser.hierarchy.node(browser.current_node()).parent;
    const auto& objects=browser.hierarchy.node(parent_node).world.objects;
    check(std::any_of(objects.begin(),objects.end(),[&](const auto& placement) { return placement.item.members==original.item.members; }),
        "Copying into a child preserves the original parent's graphic");
    write(parent/"first"/"original.txt","keep existing destination");
    browser.act({ActionKind::Paste}); settle(browser);
    check(browser.file_clipboard().has_value() && read(parent/"first"/"original.txt")=="keep existing destination",
        "Copy-paste collision keeps its clipboard and does not replace the existing file");
    browser.act({ActionKind::Up}); settle(browser); browser.act({ActionKind::Open,object(browser,"second")}); settle(browser);
    browser.act({ActionKind::Paste}); settle(browser);
    check(read(parent/"second"/"original.txt")=="clipboard source" && browser.file_clipboard().has_value(),"One copy can be pasted into multiple destinations");
    select(browser,"original.txt"); browser.act({ActionKind::Cut});
    check(browser.file_clipboard()->kind==FileOperationKind::Move,"Cut replaces a copy clipboard");
    browser.act({ActionKind::Copy}); check(browser.file_clipboard()->kind==FileOperationKind::Copy,"Copy replaces a cut clipboard");
    browser.act({ActionKind::Cancel}); check(!browser.file_clipboard(),"Escape clears a copy clipboard");
}
void rebased_transfers(const fs::path& root) {
    std::vector<fs::path> paths{root/"rebased-transfers"}; fs::create_directory(paths.front());
    for (int i=0;i<6;++i) { paths.push_back(paths.back()/"next"); fs::create_directory(paths.back()); }
    for (const auto depth:{2u,3u,5u}) {
        const auto name="transfer-"+std::to_string(depth)+".txt"; write(paths[depth]/name);
        Browser browser(paths.front()); settle(browser);
        for (unsigned level=0;level<depth;++level) { browser.act({ActionKind::Open,object(browser,"next")}); settle(browser); }
        select(browser,name); browser.act({ActionKind::Cut}); browser.act({ActionKind::Open,object(browser,"next")}); settle(browser);
        browser.act({ActionKind::Paste}); settle(browser);
        check(fs::exists(paths[depth+1]/name) && !fs::exists(paths[depth]/name),"Parent-to-child move crosses deep cached/rebased worlds");
        select(browser,name); browser.act({ActionKind::Cut}); browser.act({ActionKind::Up}); settle(browser);
        check(object(browser,name)==no_selection,"Rebased parent return does not resurrect the original source graphic");
        browser.act({ActionKind::Paste}); settle(browser);
        const auto child=browser.hierarchy.child(browser.current_node(),object(browser,"next"));
        const auto& objects=browser.hierarchy.node(child).world.objects;
        check(object(browser,name)!=no_selection && std::none_of(objects.begin(),objects.end(),[&](const auto& object) { return object.item.label==name; }),
            "Reverse paste removes the old child graphic across rebase boundaries");
    }
}
}
int main() {
    try {
        Temporary temporary;
        filesystem_actions(temporary.path); cross_filesystem(temporary.path);
        browser_actions(temporary.path); groups_and_retry(temporary.path); worker_queue(temporary.path); nested_mutations(temporary.path); parent_child_moves(temporary.path);
        copies(temporary.path); copy_clipboard(temporary.path); rebased_transfers(temporary.path);
        std::cout<<"Filesystem operations and browser confirmations/clipboard/cache tests passed\n";
    } catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
