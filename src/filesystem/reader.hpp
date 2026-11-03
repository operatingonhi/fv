#pragma once
#include "filesystem/model.hpp"
#include "filesystem/operations.hpp"
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>

namespace spatial {
struct LoadPolicy { std::size_t preview_entries=64; };
Entry read_metadata(const std::filesystem::path& path);
// One directory only; zero limit means complete enumeration. No recursive walk.
DirectoryListing read_directory(const std::filesystem::path& path,std::size_t limit=0,std::stop_token stop={});
struct LoadResult { EntryId directory; DirectoryListing listing; std::size_t limit=0; std::size_t generation=0; std::size_t scope=0; };
struct PathValidation {
    std::size_t tag=0;
    std::filesystem::path resolved;
    Entry metadata;
    std::string error;
};
PathValidation validate_path(const std::filesystem::path& path,bool directory_only);
class DirectoryLoader {
public:
    DirectoryLoader();
    ~DirectoryLoader();
    DirectoryLoader(const DirectoryLoader&)=delete;
    DirectoryLoader& operator=(const DirectoryLoader&)=delete;
    // Scope is an opaque cache namespace; entry IDs may repeat across scopes.
    void request(EntryId directory,std::filesystem::path path,std::size_t limit,std::size_t generation=0,std::size_t scope=0);
    void validate(std::size_t tag,std::filesystem::path path,bool directory_only);
    std::vector<PathValidation> poll_validations();
    void operate(FileOperation operation);
    std::vector<FileOperationResult> poll_operations();
    std::vector<LoadResult> poll();
    void discard_reads_except(std::size_t generation);
private:
    struct Request { EntryId directory=0; std::filesystem::path path; std::size_t limit=0; std::size_t generation=0, tag=0; bool validation=false, directory_only=false; std::size_t scope=0; std::optional<FileOperation> operation{}; };
    void run(std::stop_token stop);
    std::mutex mutex_;
    std::condition_variable_any wake_;
    std::deque<Request> requests_;
    std::vector<LoadResult> results_;
    std::vector<PathValidation> validations_;
    std::vector<FileOperationResult> operations_;
    std::jthread worker_;
};
}
