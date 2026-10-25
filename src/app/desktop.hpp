#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <sys/types.h>

namespace spatial {
struct LaunchPlan {
    std::vector<std::string> arguments;
    std::filesystem::path directory;
    std::string error;
};
LaunchPlan file_command(const std::filesystem::path& file);
LaunchPlan terminal_command(const std::filesystem::path& directory,std::string_view setting={});
std::string terminal_environment();
// No shell, blocking wait, renderer, or filesystem classification dependencies.
class DesktopLauncher {
public:
    ~DesktopLauncher();
    std::string launch(const LaunchPlan& plan);
    std::vector<std::string> poll();
private:
    struct Child { pid_t pid; std::string command; };
    std::vector<Child> children_;
};
}
