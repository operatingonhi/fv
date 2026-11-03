#include "filesystem/operations.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <linux/fs.h>
#include <map>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace spatial {
namespace fs=std::filesystem;
namespace {
struct Stamp {
    std::uintmax_t device,inode,size;
    mode_t mode;
    timespec modified,changed;
    bool operator==(const Stamp& other) const {
        return device==other.device && inode==other.inode && size==other.size && mode==other.mode &&
            modified.tv_sec==other.modified.tv_sec && modified.tv_nsec==other.modified.tv_nsec &&
            changed.tv_sec==other.changed.tv_sec && changed.tv_nsec==other.changed.tv_nsec;
    }
};
Stamp stamp(const fs::path& path) {
    struct stat info{};
    if (::lstat(path.c_str(),&info)!=0) throw fs::filesystem_error("Read source",path,std::error_code(errno,std::generic_category()));
    return {info.st_dev,info.st_ino,static_cast<std::uintmax_t>(info.st_size),info.st_mode,info.st_mtim,info.st_ctim};
}
FileIdentity identity(const Stamp& info) {
    return {info.device,info.inode,true,info.changed.tv_sec,info.changed.tv_nsec};
}
bool within(const fs::path& path,const fs::path& parent) {
    const auto [a,b]=std::mismatch(parent.begin(),parent.end(),path.begin(),path.end());
    (void)b; return a==parent.end();
}
void check_source(const fs::path& source,FileIdentity expected) {
    const auto info=stamp(source);
    if (!expected.known || identity(info)!=expected)
        throw std::runtime_error("Source changed, was replaced or has no verified identity; reselect it and try again");
    if (!S_ISREG(info.mode) && !S_ISDIR(info.mode) && !S_ISLNK(info.mode))
        throw std::runtime_error("Only regular files, directories and symlinks can be changed");
}
int move_exclusive(const fs::path& source,const fs::path& destination) {
    if (::syscall(SYS_renameat2,AT_FDCWD,source.c_str(),AT_FDCWD,destination.c_str(),RENAME_NOREPLACE)==0) return 0;
    return errno;
}
using Snapshot=std::map<fs::path,Stamp>;
Snapshot snapshot(const fs::path& source,std::stop_token stop) {
    Snapshot result;
    const auto add=[&](const fs::path& path) {
        if (stop.stop_requested()) throw std::runtime_error("File transfer cancelled before publication");
        const auto value=stamp(path);
        if (!S_ISREG(value.mode) && !S_ISDIR(value.mode) && !S_ISLNK(value.mode))
            throw std::runtime_error("File transfers cannot copy special files");
        result.emplace(path,value);
    };
    add(source);
    if (S_ISDIR(result.at(source).mode))
        for (const auto& entry:fs::recursive_directory_iterator(source)) add(entry.path());
    return result;
}
void copy_snapshot(const fs::path& source,const fs::path& destination,const Snapshot& entries,std::stop_token stop) {
    for (const auto& [path,value]:entries) {
        if (stop.stop_requested()) throw std::runtime_error("File transfer cancelled before publication");
        const auto relative=path.lexically_relative(source);
        const auto target=relative=="." ? destination:destination/relative;
        if (S_ISDIR(value.mode)) {
            fs::create_directory(target);
            fs::permissions(target,fs::perms::owner_all,fs::perm_options::add);
        } else if (S_ISLNK(value.mode)) fs::copy_symlink(path,target);
        else fs::copy_file(path,target,fs::copy_options::none);
    }
    // Copy timestamps after children so directory writes cannot change them again.
    for (auto it=entries.rbegin();it!=entries.rend();++it) {
        if (S_ISLNK(it->second.mode)) continue;
        const auto relative=it->first.lexically_relative(source);
        const auto target=relative=="." ? destination:destination/relative;
        fs::last_write_time(target,fs::last_write_time(it->first));
        // Moving the staged root directory to its final parent requires write
        // permission on that directory on Linux. Restore its final mode after
        // publication, before attempting any source removal.
        if (it->first!=source || !S_ISDIR(it->second.mode))
            fs::permissions(target,static_cast<fs::perms>(it->second.mode&07777));
    }
}
struct Staging {
    fs::path path;
    explicit Staging(const fs::path& parent) {
        auto pattern=(parent/".fv-move-XXXXXX").string();
        if (!::mkdtemp(pattern.data())) throw fs::filesystem_error("Create transfer staging directory",parent,std::error_code(errno,std::generic_category()));
        path=std::move(pattern);
    }
    ~Staging() {
        std::error_code error;
        // Our private copy may already have its original read-only directory
        // permissions. Restore traversal/write permission for failed-copy cleanup.
        const auto entry=path/"entry";
        if (fs::is_directory(fs::symlink_status(entry,error))) {
            fs::permissions(entry,fs::perms::owner_all,fs::perm_options::add,error);
            fs::recursive_directory_iterator it(entry,error),end;
            while (!error && it!=end) {
                if (fs::is_directory(it->symlink_status(error)))
                    fs::permissions(it->path(),fs::perms::owner_all,fs::perm_options::add,error);
                if (!error) it.increment(error);
            }
        }
        fs::remove_all(path,error);
    }
};
}
FileOperationResult perform_file_operation(FileOperation operation,std::stop_token stop) {
    FileOperationResult result; result.operation=std::move(operation);
    try {
        const auto& request=result.operation;
        if (request.source.native().find('\0')!=std::string::npos || request.destination_directory.native().find('\0')!=std::string::npos)
            throw std::runtime_error("Paths cannot contain NUL bytes");
        const auto name=request.source.filename();
        if (!request.source.is_absolute() || name.empty() || name=="." || name=="..")
            throw std::runtime_error("Select an individual filesystem entry, not a filesystem root");
        const auto source=fs::canonical(request.source.parent_path())/name;
        check_source(source,request.expected);
        result.source_parent=identity(stamp(source.parent_path()));
        if (stop.stop_requested()) throw std::runtime_error("File operation cancelled before starting");
        if (request.kind==FileOperationKind::Delete) {
            std::error_code error;
            const auto removed=fs::remove_all(source,error);
            result.changed=removed!=0 || bool(error); // Deletion can partially succeed.
            if (error) throw fs::filesystem_error("Delete failed; some contents may already have been removed",source,error);
            if (!removed) throw std::runtime_error("Source disappeared before deletion");
            result.success=true; return result;
        }
        const auto directory=fs::canonical(request.destination_directory);
        if (!fs::is_directory(directory)) throw std::runtime_error("Destination is not a directory");
        result.destination=directory/name;
        if (source==result.destination) throw std::runtime_error("Source is already in the destination directory");
        if (S_ISDIR(stamp(source).mode) && within(directory,source))
            throw std::runtime_error("A directory cannot be copied or moved into itself or its descendants");
        check_source(source,request.expected);
        if (request.kind==FileOperationKind::Move) {
            const int error=move_exclusive(source,result.destination);
            if (!error) { result.success=result.changed=true; return result; }
            if (error!=EXDEV) throw fs::filesystem_error("Move (existing destinations are never replaced)",source,result.destination,std::error_code(error,std::generic_category()));
        }
        if (fs::exists(fs::symlink_status(result.destination)))
            throw fs::filesystem_error("Destination already exists",result.destination,std::make_error_code(std::errc::file_exists));
        // Keep all copy work private until it is complete. A failed copy leaves
        // the source untouched and never exposes a partial destination.
        Staging staging(directory);
        const auto contents=snapshot(source,stop);
        const auto copy=staging.path/"entry";
        copy_snapshot(source,copy,contents,stop);
        if (snapshot(source,stop)!=contents) throw std::runtime_error("Source changed while copying; source was retained");
        const int publish_error=move_exclusive(copy,result.destination);
        if (publish_error) throw fs::filesystem_error("Publish transfer (existing destinations are never replaced)",result.destination,std::error_code(publish_error,std::generic_category()));
        result.changed=true;
        if (S_ISDIR(contents.at(source).mode))
            fs::permissions(result.destination,static_cast<fs::perms>(contents.at(source).mode&07777));
        if (request.kind==FileOperationKind::Copy) { result.success=true; return result; }
        check_source(source,request.expected);
        std::error_code removal_error;
        fs::remove_all(source,removal_error);
        if (removal_error) throw fs::filesystem_error("Source removal failed; some contents may remain",source,removal_error);
        result.success=true;
    } catch (const std::exception& error) {
        result.error=error.what();
        if (result.changed && result.operation.kind!=FileOperationKind::Delete)
            result.error+=". A complete copy is at "+result.destination.string()+"; check the source before retrying.";
    }
    return result;
}
}
