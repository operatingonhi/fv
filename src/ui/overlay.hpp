#pragma once
#include "navigation/actions.hpp"
#include "ui/presentation.hpp"

namespace spatial {
class Browser;
struct OverlayState {
    std::string path,search;
    std::size_t history_state=0;
    bool focus_path=false,focus_search=false,open_filters=false,inspector=false;
    bool open_settings=false,close_settings=false;
    bool cancel_edits=false,capture_navigation=false,release_focus=false;
};
UiAction overlay(Browser& browser,OverlayState& state,float panel_width);
void demo_caption(std::string_view text,float left,float width,float height);
void scene_heading(const Presentation& scene,float left,float width,float height);
}
