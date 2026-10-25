#pragma once
#include "navigation/actions.hpp"
#include "navigation/navigation.hpp"
#include "navigation/history.hpp"
#include "navigation/spatial_selection.hpp"
#include "ui/presentation.hpp"
#include "filesystem/mock_filesystem.hpp"
#include "filesystem/reader.hpp"
#include "app/desktop.hpp"
#include <memory>
#include <map>
#include <unordered_map>

namespace spatial {
class Browser {
public:
    Browser(); // Deterministic fixture; never launches external applications.
    explicit Browser(const std::filesystem::path& start);
    bool real_filesystem() const { return loader_!=nullptr; }
    std::int64_t now() const { return now_; }
    bool loading() const { return !pending_.empty() || !validations_.empty(); }
    const QuerySpec& query() const { return query_; }
    std::size_t matching_objects() const { return matches_.size(); }
    bool search_incomplete() const { return query_result_.incomplete; }
    bool eligible(const Item& item) const { return !query_.active() || item_matches(item,query_result_); }
    const Item* selected_item() const;
    std::string status,notice;
    Filesystem filesystem;
    LoadPolicy load_policy;
    Navigation navigation;
    History history;
    Hierarchy hierarchy;
    Layout layout;
    Presentation scene;
    Camera camera;
    bool animations=true;
    PresentationConfig presentation_config;
    double preparation_ms=0;
    std::size_t lod_evaluations=0;
    void configure_presentation() { refresh_scene(); }
    float viewport_width=800,viewport_height=600;
    std::size_t scene_revision=0;
    NodeId current_node() const { return current_; }
    void act(UiAction action);
    void update(float seconds);
private:
    struct World { std::optional<Filesystem> filesystem; std::optional<Hierarchy> hierarchy; };
    struct View {
        std::shared_ptr<World> world;
        NavigationSnapshot navigation;
        NodeId node=0;
        std::vector<NodeId> parents;
        CameraState camera;
        QuerySpec query;
    };
    enum class Purpose { Location, History, Activate, Terminal };
    struct Validation { Purpose purpose; std::size_t history_index=0,history_state=0; };
    void initialize(std::vector<DirectoryItems> directories,std::vector<Item> individuals);
    void request_directory(EntryId id,bool full,bool explicit_request=false);
    void schedule_previews();
    void update_status();
    void sync_view();
    void refresh_scene();
    void rebuild_query();
    void reconcile_selection();
    void select(std::size_t index,bool focus);
    void move(Direction direction);
    void next_match(bool reverse);
    void focus_overview();
    void save_view();
    void record_view();
    void restore_view(std::size_t state);
    void stash_world();
    void open_location(const std::filesystem::path& path);
    bool enter_existing(NodeId node);
    void validate(const std::filesystem::path& path,Validation action,bool directory_only);
    void process_validation(PathValidation result);
    void navigate(std::string text);
    void go_history(int delta);
    void terminal_here();
    std::int64_t now_=mock_now;
    std::unique_ptr<DirectoryLoader> loader_;
    std::vector<LoadResult> deferred_;
    std::map<EntryId,bool> pending_;
    bool apply_deferred_=false;
    NodeId current_=0;
    std::vector<NodeId> parents_;
    std::vector<unsigned char> levels_,scratch_levels_;
    CameraState last_lod_camera_;
    QuerySpec query_;
    QueryResult query_result_;
    bool query_dirty_=true;
    AppearanceMap appearances_;
    std::vector<std::size_t> matches_;
    std::shared_ptr<World> world_=std::make_shared<World>();
    std::unordered_map<std::size_t,View> views_;
    std::size_t next_state_=1,generation_=0,next_validation_=1,location_tag_=0;
    std::map<std::size_t,Validation> validations_;
    DesktopLauncher launcher_;
};
}
