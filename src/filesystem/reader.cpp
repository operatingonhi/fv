#include "filesystem/reader.hpp"
#include <algorithm>
#include <chrono>

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
DirectoryLoader::DirectoryLoader():worker_([this](std::stop_token stop) { run(stop); }) {}
DirectoryLoader::~DirectoryLoader() { worker_.request_stop(); wake_.notify_all(); }
void DirectoryLoader::request(EntryId directory,fs::path path,std::size_t limit) {
    std::lock_guard lock(mutex_);
    for (auto it=requests_.begin();it!=requests_.end();++it) if (it->directory==directory) {
        if (limit==0) {
            auto request=std::move(*it); request.limit=0;
            requests_.erase(it); requests_.push_front(std::move(request)); wake_.notify_one();
        }
        return;
    }
    Request request{directory,std::move(path),limit};
    if (limit==0) requests_.push_front(std::move(request));
    else requests_.push_back(std::move(request));
    wake_.notify_one();
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
        DirectoryListing listing;
        try { listing=read_directory(request.path,request.limit,stop); }
        catch (const std::exception& exception) { listing.error=exception.what(); }
        if (stop.stop_requested()) return;
        std::lock_guard lock(mutex_);
        results_.push_back({request.directory,std::move(listing),request.limit});
    }
}
}
