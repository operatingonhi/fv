#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "app/desktop.hpp"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <spawn.h>
#include <sys/wait.h>
#include <system_error>

extern char** environ;
namespace spatial {
LaunchPlan file_command(const std::filesystem::path& file) {
    if (!file.is_absolute() || file.string().find('\0')!=std::string::npos) return {{},{},"Opening a file requires an absolute path"};
    return {{"xdg-open",file.string()},file.parent_path(),{}};
}
LaunchPlan terminal_command(const std::filesystem::path& directory,std::string_view setting) {
    if (!directory.is_absolute() || directory.string().find('\0')!=std::string::npos) return {{},{},"Terminal requires an absolute directory"};
    LaunchPlan plan; plan.directory=directory;
    if (setting.empty()) { plan.arguments={"xdg-terminal-exec"}; return plan; }
    std::string token;
    char quote=0; bool escaped=false,started=false;
    for (char c:setting) {
        if (c=='\0') { plan.error="Terminal command contains NUL"; return plan; }
        if (escaped) { token+=c; escaped=false; started=true; continue; }
        if (c=='\\' && quote!='\'') { escaped=true; started=true; continue; }
        if (quote) { if (c==quote) quote=0; else token+=c; started=true; continue; }
        if (c=='\'' || c=='"') { quote=c; started=true; continue; }
        if (c==' ' || c=='\t') {
            if (started) { plan.arguments.push_back(token); token.clear(); started=false; }
        } else { token+=c; started=true; }
    }
    if (escaped || quote) plan.error="Unclosed quote or escape in terminal command";
    if (started) plan.arguments.push_back(token);
    if (plan.arguments.empty() || plan.arguments.front().empty()) plan.error="Terminal command is empty";
    return plan;
}
std::string terminal_environment() {
    for (const auto* name:{"FSN_TERMINAL","TERMINAL"}) if (const char* value=std::getenv(name); value && *value) return value;
    return {};
}
DesktopLauncher::~DesktopLauncher() { poll(); } // Running external applications outlive the browser.
std::string DesktopLauncher::launch(const LaunchPlan& plan) {
    if (!plan.error.empty()) return plan.error;
    if (plan.arguments.empty()) return "No launch command";
    std::vector<char*> arguments;
    for (const auto& arg:plan.arguments) arguments.push_back(const_cast<char*>(arg.c_str()));
    arguments.push_back(nullptr);
    posix_spawn_file_actions_t actions;
    int error=posix_spawn_file_actions_init(&actions);
    if (error) return std::system_category().message(error);
    error=posix_spawn_file_actions_addchdir_np(&actions,plan.directory.c_str());
    // GNU/Linux closefrom avoids leaking X11/GL/loader descriptors into applications.
    if (!error) error=posix_spawn_file_actions_addclosefrom_np(&actions,3);
    pid_t pid=0;
    if (!error) error=posix_spawnp(&pid,arguments.front(),&actions,nullptr,arguments.data(),environ);
    posix_spawn_file_actions_destroy(&actions);
    if (error) return plan.arguments.front()+": "+std::system_category().message(error);
    children_.push_back({pid,plan.arguments.front()}); return {};
}
std::vector<std::string> DesktopLauncher::poll() {
    std::vector<std::string> errors;
    for (auto it=children_.begin();it!=children_.end();) {
        int status=0;
        const auto result=waitpid(it->pid,&status,WNOHANG);
        if (result==0 || (result<0 && errno==EINTR)) { ++it; continue; }
        if (result>0 && (!WIFEXITED(status) || WEXITSTATUS(status)!=0))
            errors.push_back(it->command+": application launcher failed"+(WIFEXITED(status) ? " (exit "+std::to_string(WEXITSTATUS(status))+")":" (signal)"));
        if (result<0 && errno!=ECHILD) errors.push_back(it->command+": "+std::strerror(errno));
        it=children_.erase(it);
    }
    return errors;
}
}
