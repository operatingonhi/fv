#pragma once
#include "filesystem/model.hpp"
#include <stop_token>

namespace spatial {
enum class FileOperationKind { Move, Delete };
struct FileOperation {
    FileOperationKind kind=FileOperationKind::Move;
    std::filesystem::path source,destination_directory;
    FileIdentity expected;
};
struct FileOperationResult {
    FileOperation operation;
    std::filesystem::path destination;
    bool success=false,changed=false;
    std::string error;
};
// Worker-only I/O. Move never overwrites; symlinks are entries, not their targets.
// Cross-device moves publish a complete staged copy before removing the source.
FileOperationResult perform_file_operation(FileOperation operation,std::stop_token stop={});
}
