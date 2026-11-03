#include "filesystem/reader.hpp"
#include <algorithm>
#include <chrono>
#include <sys/stat.h>

namespace spatial {
namespace fs=std::filesystem;
namespace {
void error(Entry& entry,const char* operation,const std::error_code& code) {
    if (!entry.error.empty()) entry.error+="; ";
    entry.error+=std::string(operation)+": "+code.message();
}
}
Entry read_metadata(const fs::path& path) {
    Entry result;
    result.name=path.filename().string(); result.full_path=path.string();
    result.hidden=result.name.starts_with('.'); result.modified_known=false;
    std::error_code ec;
    const auto status=fs::symlink_status(path,ec);
    if (ec || !fs::exists(status)) {
        error(result,"status",ec ? ec:std::make_error_code(std::errc::no_such_file_or_directory));
        return result;
    }
    struct stat identity{};
    if (::lstat(path.c_str(),&identity)==0)
        result.identity={identity.st_dev,identity.st_ino,true,identity.st_ctim.tv_sec,identity.st_ctim.tv_nsec};
    result.permissions=status.permissions();
    if (fs::is_symlink(status)) {
        result.kind=EntryKind::Symlink;
        result.link_target=fs::read_symlink(path,ec).string();
        if (ec) error(result,"read link",ec);
        const auto target=fs::status(path,ec);
        result.broken_link=ec || !fs::exists(target);
        if (result.broken_link) error(result,"link target",ec ? ec:std::make_error_code(std::errc::no_such_file_or_directory));
        // No target traversal or target size/mtime masquerading as link metadata.
        return result;
    }
    result.kind=fs::is_directory(status) ? EntryKind::Directory:fs::is_regular_file(status) ? EntryKind::File:EntryKind::Other;
    const auto execute=fs::perms::owner_exec|fs::perms::group_exec|fs::perms::others_exec;
    result.executable=result.kind==EntryKind::File && (status.permissions()&execute)!=fs::perms::none;
    const auto modified=fs::last_write_time(path,ec);
    if (ec) error(result,"modified time",ec);
    else {
        result.modified=std::chrono::duration_cast<std::chrono::seconds>(fs::file_time_type::clock::to_sys(modified).time_since_epoch()).count();
        result.modified_known=true;
    }
    if (result.kind==EntryKind::File) {
        result.bytes=fs::file_size(path,ec);
        result.size_known=!ec;
        if (ec) error(result,"size",ec);
    }
    return result;
}
DirectoryListing read_directory(const fs::path& path,std::size_t limit,std::stop_token stop) {
    DirectoryListing result;
    result.directory=read_metadata(path);
    if (result.directory.name.empty() || result.directory.name=="." || result.directory.name=="..") {
        // Resolve display identity on the worker. Keep the original I/O path:
        // lexical normalization of symlink/.. would change its meaning.
        std::error_code name_error;
        const auto resolved=fs::canonical(path,name_error);
        if (!name_error) result.directory.name=resolved.filename().string();
        if (result.directory.name.empty()) result.directory.name=path.string();
        result.directory.hidden=result.directory.name.starts_with('.');
    }
    std::error_code ec;
    // An explicitly supplied root may be a directory symlink; discovered links
    // remain symlinks and are never queued for traversal.
    fs::directory_iterator it(path,ec), end;
    if (ec) { result.error="Read directory: "+ec.message(); return result; }
    if (result.directory.kind==EntryKind::Symlink) result.directory.kind=EntryKind::Directory;
    while (it!=end) {
        if (stop.stop_requested()) { result.complete=false; return result; }
        if (limit && result.entries.size()>=limit) { result.complete=false; break; }
        result.entries.push_back(read_metadata(it->path()));
        it.increment(ec);
        if (ec) { result.error="Continue directory: "+ec.message(); break; }
    }
    std::sort(result.entries.begin(),result.entries.end(),[](const Entry& a,const Entry& b) { return a.name<b.name; });
    return result;
}
PathValidation validate_path(const fs::path& path,bool directory_only) {
    PathValidation result;
    std::error_code ec;
    result.resolved=fs::canonical(path,ec);
    if (ec) { result.error="Resolve path: "+ec.message(); return result; }
    result.metadata=read_metadata(result.resolved);
    if (!result.metadata.error.empty()) { result.error=result.metadata.error; return result; }
    if (result.metadata.kind==EntryKind::Directory) {
        fs::directory_iterator probe(result.resolved,ec);
        if (ec) result.error="Read directory: "+ec.message();
    } else if (directory_only) result.error="Path is not a directory";
    else if (result.metadata.kind!=EntryKind::File) result.error="Path is not a regular file or directory";
    return result;
}
DirectoryLoader::DirectoryLoader():worker_([this](std::stop_token stop) { run(stop); }) {}
DirectoryLoader::~DirectoryLoader() { worker_.request_stop(); wake_.notify_all(); }
void DirectoryLoader::request(EntryId directory,fs::path path,std::size_t limit,std::size_t generation,std::size_t scope) {
    std::lock_guard lock(mutex_);
    for (auto it=requests_.begin();it!=requests_.end();++it) if (!it->validation && !it->operation && it->directory==directory && it->generation==generation && it->scope==scope) {
        if (limit==0) {
            auto request=std::move(*it); request.limit=0;
            requests_.erase(it); requests_.push_front(std::move(request)); wake_.notify_one();
        }
        return;
    }
    Request request{directory,std::move(path),limit,generation};
    request.scope=scope;
    if (limit==0) requests_.push_front(std::move(request));
    else requests_.push_back(std::move(request));
    wake_.notify_one();
}
void DirectoryLoader::validate(std::size_t tag,fs::path path,bool directory_only) {
    std::lock_guard lock(mutex_);
    Request request{0,std::move(path),0}; request.validation=true; request.tag=tag; request.directory_only=directory_only;
    requests_.push_front(std::move(request)); wake_.notify_one();
}
std::vector<PathValidation> DirectoryLoader::poll_validations() {
    std::lock_guard lock(mutex_);
    std::vector<PathValidation> result; result.swap(validations_); return result;
}
void DirectoryLoader::operate(FileOperation operation) {
    std::lock_guard lock(mutex_);
    Request request; request.operation=std::move(operation);
    requests_.push_front(std::move(request)); wake_.notify_one();
}
std::vector<FileOperationResult> DirectoryLoader::poll_operations() {
    std::lock_guard lock(mutex_);
    std::vector<FileOperationResult> result; result.swap(operations_); return result;
}
void DirectoryLoader::discard_reads_except(std::size_t generation) {
    std::lock_guard lock(mutex_);
    std::erase_if(requests_,[&](const Request& request) { return !request.validation && !request.operation && request.generation!=generation; });
}
std::vector<LoadResult> DirectoryLoader::poll() {
    std::lock_guard lock(mutex_);
    std::vector<LoadResult> result;
    result.swap(results_);
    return result;
}
void DirectoryLoader::run(std::stop_token stop) {
    while (!stop.stop_requested()) {
        Request request;
        {
            std::unique_lock lock(mutex_);
            if (!wake_.wait(lock,stop,[&] { return !requests_.empty(); })) return;
            request=std::move(requests_.front()); requests_.pop_front();
        }
        if (request.operation) {
            auto result=perform_file_operation(std::move(*request.operation),stop);
            std::lock_guard lock(mutex_); operations_.push_back(std::move(result)); continue;
        }
        if (request.validation) {
            PathValidation validation;
            try { validation=validate_path(request.path,request.directory_only); }
            catch (const std::exception& exception) { validation.error=exception.what(); }
            validation.tag=request.tag;
            if (stop.stop_requested()) return;
            std::lock_guard lock(mutex_); validations_.push_back(std::move(validation)); continue;
        }
        DirectoryListing listing;
        try { listing=read_directory(request.path,request.limit,stop); }
        catch (const std::exception& exception) { listing.error=exception.what(); }
        if (stop.stop_requested()) return;
        std::lock_guard lock(mutex_);
        results_.push_back({request.directory,std::move(listing),request.limit,request.generation,request.scope});
    }
}
}
