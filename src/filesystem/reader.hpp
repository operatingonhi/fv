#pragma once
#include "filesystem/model.hpp"
#include <condition_variable>
#include <deque>
#include <mutex>
#include <stop_token>
#include <thread>

namespace spatial {
struct LoadPolicy { std::size_t preview_entries=64, preview_directories=8; };
Entry read_metadata(const std::filesystem::path& path);
// One directory only; zero limit means complete enumeration. No recursive walk.
DirectoryListing read_directory(const std::filesystem::path& path,std::size_t limit=0,std::stop_token stop={});
struct LoadResult { EntryId directory; DirectoryListing listing; std::size_t limit=0; };
class DirectoryLoader {
public:
    DirectoryLoader();
    ~DirectoryLoader();
    DirectoryLoader(const DirectoryLoader&)=delete;
    DirectoryLoader& operator=(const DirectoryLoader&)=delete;
    void request(EntryId directory,std::filesystem::path path,std::size_t limit);
    std::vector<LoadResult> poll();
private:
    struct Request { EntryId directory; std::filesystem::path path; std::size_t limit; };
    void run(std::stop_token stop);
    std::mutex mutex_;
    std::condition_variable_any wake_;
    std::deque<Request> requests_;
    std::vector<LoadResult> results_;
    std::jthread worker_;
};
}
