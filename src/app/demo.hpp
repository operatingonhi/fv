#pragma once
#include "app/browser.hpp"
#include "ui/overlay.hpp"
#include <fstream>
#include <optional>
#include <string_view>

namespace spatial {
inline constexpr int demo_depth=4;
struct DemoWindowSize { int width,height; };
std::optional<DemoWindowSize> demo_window_size(std::string_view text);
struct DemoStep {
    enum class Kind { Hold, Focus, Open, Up, Group, Orbit, Zoom, Reset, Next, Previous,
        Search, Match, Inspect, FilterArtifacts, HideHidden, ClearFilters, ExplainFile, Terminal,
        Back, Forward, Navigate, Settings, CloseSettings };
    enum class Narration { Feature, Title, Continue, Silent };
    Kind kind;
    std::string target,description;
    float seconds=3,value=0;
    Narration narration=Narration::Feature;
};
std::vector<DemoStep> demo_steps();
std::string demo_script();
// Only creates new fixture files; refuses an existing fixture or output script.
std::filesystem::path prepare_demo(const std::filesystem::path& output);
void write_demo_script(const std::filesystem::path& file);
class DemoTour {
public:
    explicit DemoTour(const std::filesystem::path& output,bool external=true);
    void update(Browser& browser,OverlayState& overlay,float seconds);
    bool done() const { return index_==steps_.size(); }
    std::string caption() const;
    std::size_t completed() const { return index_; }
private:
    void begin(Browser& browser,OverlayState& overlay);
    std::vector<DemoStep> steps_;
    std::string caption_,feature_caption_;
    std::ofstream log_;
    std::size_t index_=0,feature_=0,feature_count_=0;
    bool begun_=false,external_=true,check_launch_=false;
    float elapsed_=0,held_=0,wait_=0;
};
}
