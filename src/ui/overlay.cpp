#include "ui/overlay.hpp"
#include "app/browser.hpp"
#include "graphics/picking.hpp"
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <array>
#include <algorithm>
#include <ctime>

namespace spatial {
namespace {
const char* entry_kind(EntryKind kind) {
    switch (kind) {
    case EntryKind::Directory: return "directory";
    case EntryKind::File: return "regular file";
    case EntryKind::Symlink: return "symlink";
    default: return "special/unknown";
    }
}
void selection_summary(const Browser& browser) {
    const auto* item=browser.selected_item();
    if (!item) { ImGui::TextUnformatted("No matching selection on this page"); return; }
    ImGui::TextWrapped("%s — %s",item->label.c_str(),category_name(item->category));
    if (item->group!=GroupKind::None) {
        ImGui::Text("%zu members",item->members.size());
        return;
    }
    const auto& entry=browser.filesystem.entry(item->members.front());
    if (entry.size_known) ImGui::Text("%llu bytes",static_cast<unsigned long long>(entry.bytes));
    else ImGui::TextUnformatted("Size: unavailable");
    if (entry.modified_known) {
        const auto timestamp=static_cast<std::time_t>(entry.modified);
        std::tm local{}; char text[32]{};
        if (localtime_r(&timestamp,&local) && std::strftime(text,sizeof(text),"%Y-%m-%d %H:%M",&local)) {
            ImGui::Text("Modified: %s",text);
            return;
        }
    }
    ImGui::TextUnformatted("Modified: unavailable");
}
void inspector(const Browser& browser) {
    const auto* item=browser.selected_item();
    if (!item) { ImGui::TextUnformatted("No object selected"); return; }
    ImGui::TextWrapped("%s",item->label.c_str());
    ImGui::Text("Semantic: %s / salience %.2f",category_name(item->category),item->salience);
    if (!item->explanation.empty()) ImGui::TextWrapped("%s",item->explanation.c_str());
    if (item->group!=GroupKind::None) {
        ImGui::Text("%zu members",item->members.size());
        ImGui::TextWrapped("Directory: %s",browser.filesystem.path(browser.navigation.context().directory).c_str());
        if (!item->members.empty()) {
            ImGui::TextWrapped("First: %s",browser.filesystem.entry(item->members.front()).name.c_str());
            ImGui::TextWrapped("Last: %s",browser.filesystem.entry(item->members.back()).name.c_str());
        }
        ImGui::TextUnformatted("Enter to inspect individual members"); return;
    }
    const auto& entry=browser.filesystem.entry(item->members.front());
    ImGui::TextWrapped("Path: %s",entry.full_path.c_str());
    if (ImGui::Button("Copy path")) ImGui::SetClipboardText(entry.full_path.c_str());
    ImGui::Text("Type: %s%s",entry_kind(entry.kind),entry.hidden ? " (hidden)":"");
    if (entry.size_known) ImGui::Text("Size: %llu bytes",static_cast<unsigned long long>(entry.bytes));
    else ImGui::TextUnformatted("Size: unavailable");
    if (entry.modified_known) {
        const auto timestamp=static_cast<std::time_t>(entry.modified);
        std::tm local{}; char text[96]{};
        if (localtime_r(&timestamp,&local)) std::strftime(text,sizeof(text),"%Y-%m-%d %H:%M:%S %Z",&local);
        ImGui::Text("Modified: %s",text);
    } else ImGui::TextUnformatted("Modified: unavailable");
    if (entry.permissions!=std::filesystem::perms::unknown)
        ImGui::Text("Permissions: %04o%s",static_cast<unsigned>(entry.permissions)&07777u,entry.executable ? " executable":"");
    if (entry.kind==EntryKind::Symlink) ImGui::TextWrapped("Target: %s%s",entry.link_target.c_str(),entry.broken_link ? " (unavailable)":"");
    if (!entry.error.empty()) ImGui::TextWrapped("%s",entry.error.c_str());
    if (entry.kind==EntryKind::Directory && !browser.filesystem.state(entry.id).message.empty())
        ImGui::TextWrapped("%s",browser.filesystem.state(entry.id).message.c_str());
}
}
UiAction overlay(Browser& browser,OverlayState& state,float panel_width) {
    const auto& fs=browser.filesystem; const auto& nav=browser.navigation; const auto& layout=browser.layout;
    UiAction action;
    const auto history_state=browser.history.current().state;
    const bool context_changed=state.history_state!=0 && state.history_state!=history_state;
    const bool reset_inputs=state.history_state!=history_state || state.cancel_edits;
    if (reset_inputs) {
        state.path=fs.path(nav.context().directory); state.search=browser.query().text; state.history_state=history_state;
    }
    if (!state.focus_search && state.search!=browser.query().text && !ImGui::GetIO().WantTextInput) state.search=browser.query().text;
    ImGui::SetNextWindowPos({8,8},ImGuiCond_Always);
    ImGui::SetNextWindowSize({panel_width-16,ImGui::GetIO().DisplaySize.y-16},ImGuiCond_Always);
    ImGui::Begin("Spatial navigator",nullptr,ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoTitleBar);
    ImGui::BeginDisabled(!browser.history.can_back());
    if (ImGui::Button("Back")) action={ActionKind::Back};
    ImGui::EndDisabled(); ImGui::SameLine();
    ImGui::BeginDisabled(!browser.history.can_forward());
    if (ImGui::Button("Forward")) action={ActionKind::Forward};
    ImGui::EndDisabled(); ImGui::SameLine();
    if (ImGui::Button("Parent")) action={ActionKind::Up};
    ImGui::SetNextItemWidth(-36);
    if (state.focus_path) { ImGui::SetKeyboardFocusHere(); state.focus_path=false; }
    if (ImGui::InputText("##path",&state.path,ImGuiInputTextFlags_EnterReturnsTrue|ImGuiInputTextFlags_AutoSelectAll) && !state.cancel_edits)
        action={ActionKind::Navigate,0,state.path};
    ImGui::SameLine(); if (ImGui::Button("Go")) action={ActionKind::Navigate,0,state.path};
    ImGui::SetNextItemWidth(-1);
    if (state.focus_search) { ImGui::SetKeyboardFocusHere(); state.focus_search=false; }
    const bool search_submit=ImGui::InputTextWithHint("##search","Search cached names (Ctrl+F)",&state.search,ImGuiInputTextFlags_EnterReturnsTrue);
    if (!reset_inputs && state.search!=browser.query().text) action={ActionKind::Search,0,state.search};
    if (search_submit && !reset_inputs) { action={ActionKind::SearchActivate,0,state.search}; state.release_focus=true; }
    if (state.open_filters) { ImGui::OpenPopup("Filters"); state.open_filters=false; }
    if (ImGui::Button(browser.query().filter==FilterSpec{} ? "Filters":"Filters *")) ImGui::OpenPopup("Filters");
    ImGui::SameLine(); ImGui::Text("%zu matching objects",browser.matching_objects());
    if (ImGui::BeginPopup("Filters")) {
        auto filter=browser.query().filter; bool changed=false;
        int kind=static_cast<int>(filter.kind);
        if (ImGui::Combo("Entries",&kind,"All\0Files\0Directories\0")) { filter.kind=static_cast<EntryFilter>(kind); changed=true; }
        const char* categories[]={"Any semantic type","Directory","Text","Source","Image","Media","Archive","Executable","Binary","Generated","Symlink","Special"};
        int category=filter.category ? static_cast<int>(*filter.category)+1:0;
        if (ImGui::Combo("Type",&category,categories,static_cast<int>(std::size(categories)))) {
            filter.category=category==0 ? std::nullopt:std::optional{static_cast<Category>(category-1)}; changed=true;
        }
        changed=ImGui::Checkbox("Include hidden",&filter.include_hidden) || changed;
        changed=ImGui::Checkbox("Include build/cache artifacts",&filter.include_artifacts) || changed;
        if (ImGui::Button("Clear filters")) { filter=FilterSpec{}; changed=true; }
        if (changed) { action={ActionKind::Filter}; action.filter=filter; }
        ImGui::TextWrapped("Excluded content is dimmed; matching loaded contents keep directory context navigable.");
        ImGui::EndPopup();
    }
    if (browser.query().active()) {
        if (ImGui::Button("Previous match")) action={ActionKind::PreviousMatch};
        ImGui::SameLine();
        if (ImGui::Button("Next match")) action={ActionKind::NextMatch};
        ImGui::TextWrapped(browser.search_incomplete() ? "Cached scope only; unloaded contents not searched":"Search: current context + two cached levels");
    }
    ImGui::Text("Loaded: %zu / page %zu of %zu",layout.total_entries,layout.page+1,layout.pages);
    ImGui::TextWrapped("%s",browser.status.c_str());
    if (!browser.notice.empty()) ImGui::TextWrapped("%s",browser.notice.c_str());
    ImGui::BeginDisabled(layout.page==0); if (ImGui::Button("< Page")) action={ActionKind::PreviousPage}; ImGui::EndDisabled();
    ImGui::SameLine(); ImGui::BeginDisabled(layout.page+1>=layout.pages); if (ImGui::Button("Page >")) action={ActionKind::NextPage}; ImGui::EndDisabled();
    // Reserve space in the same sidebar for the summary, controls and optional
    // inspector. Both lists scroll independently; no floating window is needed.
    const auto detail_height=state.inspector ? 200.0f:0.0f;
    const auto height=std::max(70.0f,ImGui::GetContentRegionAvail().y-180-detail_height);
    if (ImGui::BeginChild("Objects",{0,height},ImGuiChildFlags_Borders)) {
        for (std::size_t i=0;i<layout.objects.size();++i) {
            const auto& item=layout.objects[i].item;
            const bool eligible=browser.eligible(item);
            auto color=semantic_color(item.category)*(eligible ? 1.0f:0.25f);
            ImGui::PushID(static_cast<int>(i)); ImGui::PushStyleColor(ImGuiCol_Text,ImVec4{color.x,color.y,color.z,1});
            ImGui::BeginDisabled(!eligible);
            auto label=display_name(item.label)+(item.group==GroupKind::None ? "":" ["+std::to_string(item.members.size())+"]");
            if (item.group==GroupKind::None) {
                const auto& entry=fs.entry(item.members.front());
                if (!entry.error.empty() || fs.state(entry.id).state==LoadState::Error) label+=" (unavailable)";
            }
            const auto position=ImGui::GetCursorScreenPos();
            if (ImGui::Selectable("##entry",i==nav.selected(),ImGuiSelectableFlags_AllowDoubleClick))
                action={ImGui::IsMouseDoubleClicked(0) ? ActionKind::Open:ActionKind::Focus,i};
            ImGui::GetWindowDrawList()->AddText(position,ImGui::GetColorU32(ImGuiCol_Text),label.c_str());
            ImGui::EndDisabled(); ImGui::PopStyleColor(); ImGui::PopID();
        }
    }
    ImGui::EndChild();
    ImGui::Separator();
    selection_summary(browser);
    if (ImGui::Button(state.inspector ? "Less detail":"Inspect")) state.inspector=!state.inspector;
    ImGui::SameLine(); if (ImGui::Button("Terminal here")) action={ActionKind::Terminal};
    if (ImGui::Button("Activate (Enter)")) action={ActionKind::Open,nav.selected()};
    ImGui::SameLine(); if (ImGui::Button("Reset (R)")) action={ActionKind::Reset};
    if (ImGui::Button("Settings")) ImGui::OpenPopup("Settings");
    if (ImGui::BeginPopup("Settings")) {
        ImGui::TextUnformatted("Session settings");
        ImGui::Checkbox("Animate navigation",&browser.animations);
        ImGui::SliderFloat("Duration (seconds)",&browser.camera.animation_duration,0.0f,0.8f,"%.2f");
        ImGui::SliderFloat("Orbit sensitivity",&browser.camera.orbit_sensitivity,0.2f,3.0f);
        ImGui::SliderFloat("Zoom sensitivity",&browser.camera.zoom_sensitivity,0.2f,3.0f);
        int depth=static_cast<int>(browser.presentation_config.nested_depth);
        int labels=static_cast<int>(browser.presentation_config.label_limit);
        bool changed=ImGui::SliderInt("Nested detail depth",&depth,1,6);
        changed=ImGui::SliderInt("Label density",&labels,0,20) || changed;
        if (changed) {
            browser.presentation_config.nested_depth=static_cast<unsigned>(depth);
            browser.presentation_config.label_limit=static_cast<std::size_t>(labels);
            browser.configure_presentation();
        }
        ImGui::Text("Preparation: %.3f ms / %zu boxes / %zu labels",browser.preparation_ms,browser.scene.boxes.size(),browser.scene.labels.size());
        ImGui::TextWrapped("Changes apply this session. Hidden entries are controlled in Filters.");
        ImGui::EndPopup();
    }
    if (state.inspector) {
        ImGui::SeparatorText("Metadata");
        if (ImGui::BeginChild("Metadata details",{0,200},ImGuiChildFlags_Borders)) inspector(browser);
        ImGui::EndChild();
    }
    state.capture_navigation=ImGui::IsAnyItemActive() || ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId) || ImGui::GetIO().NavVisible;
    ImGui::End();
    if (state.cancel_edits || state.release_focus || context_changed) {
        ImGui::SetWindowFocus(nullptr); state.capture_navigation=false; state.release_focus=false;
        if (reset_inputs) { state.path=fs.path(nav.context().directory); state.search=browser.query().text; }
    }
    state.cancel_edits=false;
    return action;
}
void scene_labels(const Presentation& scene,const Camera& camera,float left,float width,float height) {
    if (width<=0 || height<=0) return;
    auto* draw=ImGui::GetBackgroundDrawList();
    draw->PushClipRect({left,0},{left+width,height});
    const auto matrix=camera.view_projection(width/height);
    std::array<ImVec4,32> occupied{};
    std::size_t count=0;
    // Selected label wins collisions; other labels are progressively disclosed.
    for (bool selected:{true,false}) for (const auto& label:scene.labels) {
        if (!label.visible || label.selected!=selected) continue;
        const auto point=project(label.anchor,matrix,width,height);
        if (!point) continue;
        const auto edge=project(label.anchor+Vec3{label.extent,0,0},matrix,width,height);
        if (!label.selected && (!edge || std::abs(edge->x-point->x)<12.0f)) continue;
        // Long selected names stay visible inside the spatial viewport. The
        // sidebar inspector retains the complete filename and path.
        const char* text=label.text.c_str();
        std::string shortened;
        auto size=ImGui::CalcTextSize(text);
        if (label.selected && size.x>width-12) {
            shortened=label.text;
            const auto suffix_width=ImGui::CalcTextSize("...").x;
            while (!shortened.empty() && ImGui::CalcTextSize(shortened.c_str()).x+suffix_width>width-12) {
                auto end=shortened.size()-1;
                while (end>0 && (static_cast<unsigned char>(shortened[end])&0xc0)==0x80) --end;
                shortened.resize(end);
            }
            shortened+="..."; text=shortened.c_str(); size=ImGui::CalcTextSize(text);
        }
        ImVec2 pos{left+point->x-size.x/2,point->y-size.y};
        if (label.selected) {
            pos.x=std::clamp(pos.x,left+3,std::max(left+3,left+width-size.x-3));
            pos.y=std::clamp(pos.y,2.0f,std::max(2.0f,height-size.y-2));
        }
        const ImVec4 rect{pos.x-3,pos.y-2,pos.x+size.x+3,pos.y+size.y+2};
        if (rect.x<left || rect.z>left+width || rect.y<0 || rect.w>height) continue;
        bool overlaps=false;
        for (std::size_t i=0;i<count;++i)
            if (rect.x<occupied[i].z && rect.z>occupied[i].x && rect.y<occupied[i].w && rect.w>occupied[i].y)
                overlaps=true;
        if (overlaps) continue;
        if (count<occupied.size()) occupied[count++]=rect;
        draw->AddRectFilled({rect.x,rect.y},{rect.z,rect.w},IM_COL32(4,7,10,220));
        draw->AddText(pos,ImGui::ColorConvertFloat4ToU32({label.color.x,label.color.y,label.color.z,1}),text);
    }
    draw->PopClipRect();
}
}
