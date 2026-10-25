#pragma once
#include "ui/overlay.hpp"
#include "filesystem/mock_filesystem.hpp"
#include "filesystem/reader.hpp"
#include <memory>
#include <map>

namespace spatial {
// Coordinates live in one persistent hierarchy. Navigation only changes the
// active node and asks the existing camera to move to its world-space transform.
class Browser {
public:
    Browser(); // Deterministic fixture for non-graphical regression tests.
    explicit Browser(const std::filesystem::path& start);
    bool real_filesystem() const { return loader_!=nullptr; }
    std::int64_t now() const { return now_; }
    bool loading() const { return !pending_.empty(); }
    std::string status;
    Filesystem filesystem;
    LoadPolicy load_policy;
    Navigation navigation;
    Hierarchy hierarchy;
    Layout layout;
    Presentation scene;
    Camera camera;
    bool animations=true;
    std::size_t scene_revision=0;
    NodeId current_node() const { return current_; }
    void act(UiAction action);
    void update(float seconds);
private:
    void initialize(std::vector<DirectoryItems> directories,std::vector<Item> individuals);
    void request_directory(EntryId id,bool full,bool explicit_request=false);
    void schedule_previews();
    void update_status();
    void sync_view();
    void refresh_scene();
    void focus_overview();
    std::int64_t now_=mock_now;
    std::unique_ptr<DirectoryLoader> loader_;
    std::vector<LoadResult> deferred_;
    std::map<EntryId,bool> pending_; // true means a complete enumeration is queued.
    bool apply_deferred_=false;
    NodeId current_=0;
    std::vector<NodeId> parents_;
    std::vector<unsigned char> levels_,scratch_levels_;
};
}
