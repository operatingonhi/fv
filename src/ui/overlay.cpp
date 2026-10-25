#include "ui/overlay.hpp"
#include "graphics/picking.hpp"
#include <imgui.h>
#include <array>
#include <algorithm>

namespace spatial {
UiAction overlay(const Filesystem& fs,const Navigation& navigation,const Layout& layout,
    const Camera& camera,bool& animations,float panel_width,std::int64_t now,const std::string& status,bool real) {
    UiAction action;
    ImGui::SetNextWindowPos({8,8},ImGuiCond_Always);
    ImGui::SetNextWindowSize({panel_width-16,ImGui::GetIO().DisplaySize.y-16},ImGuiCond_Always);
    ImGui::Begin("Spatial navigator",nullptr,ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse);
    ImGui::TextUnformatted(real ? "MILESTONE 3 / FILESYSTEM":"MOCK REGRESSION FIXTURE");
    const auto path=navigation.path(fs);
    ImGui::TextWrapped("%s",path.c_str());
    ImGui::Text("Loaded: %zu entries / %zu objects",layout.total_entries,layout.objects.size());
    ImGui::Text("Camera: %s",camera.moving() ? "moving":"ready");
    ImGui::TextWrapped("%s",status.c_str());
    ImGui::BeginDisabled(!navigation.can_go_up());
    if (ImGui::Button("Parent (Backspace)")) action={ActionKind::Up};
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Reset (R)")) action={ActionKind::Reset};
    ImGui::BeginDisabled(layout.page==0);
    if (ImGui::Button("<")) action={ActionKind::PreviousPage};
    ImGui::EndDisabled(); ImGui::SameLine();
    ImGui::Text("Page %zu / %zu",layout.page+1,layout.pages); ImGui::SameLine();
    ImGui::BeginDisabled(layout.page+1>=layout.pages);
    if (ImGui::Button(">")) action={ActionKind::NextPage};
    ImGui::EndDisabled();
    const float list_height=std::max(80.0f,ImGui::GetContentRegionAvail().y-260.0f);
    if (ImGui::BeginChild("Objects",{0,list_height},ImGuiChildFlags_Borders)) {
        for (std::size_t i=0;i<layout.objects.size();++i) {
            const auto& item=layout.objects[i].item;
            const auto color=semantic_color(item.category);
            ImGui::PushID(static_cast<int>(i));
            ImGui::PushStyleColor(ImGuiCol_Text,ImVec4{color.x,color.y,color.z,1});
            auto label=item.label+(item.group==GroupKind::None ? "":" ["+std::to_string(item.members.size())+"]");
            if (item.group==GroupKind::None) {
                const auto& entry=fs.entry(item.members.front());
                if (!entry.error.empty() || fs.state(entry.id).state==LoadState::Error) label+=" (unavailable)";
            }
            const auto text_position=ImGui::GetCursorScreenPos();
            if (ImGui::Selectable("##entry",i==navigation.selected(),ImGuiSelectableFlags_AllowDoubleClick))
                action={ImGui::IsMouseDoubleClicked(0) ? ActionKind::Open:ActionKind::Focus,i};
            // Filenames containing ## are text, not ImGui widget identifiers.
            ImGui::GetWindowDrawList()->AddText(text_position,ImGui::GetColorU32(ImGuiCol_Text),label.c_str());
            ImGui::PopStyleColor();
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    if (navigation.selected()<layout.objects.size()) {
        const auto& item=layout.objects[navigation.selected()].item;
        ImGui::TextWrapped("%s",item.label.c_str());
        ImGui::Text("%s / salience %.2f",category_name(item.category),item.salience);
        if (!item.explanation.empty()) ImGui::TextWrapped("%s",item.explanation.c_str());
        if (item.group!=GroupKind::None) ImGui::Text("%zu members (expand to inspect)",item.members.size());
        else {
            const auto& entry=fs.entry(item.members.front());
            if (entry.modified_known) ImGui::Text("Modified: %lld hours ago",static_cast<long long>(
                std::max<std::int64_t>(0,now-entry.modified)/3600));
            else ImGui::TextUnformatted("Modified: unavailable");
            if (entry.size_known) ImGui::Text("Size: %llu bytes",static_cast<unsigned long long>(entry.bytes));
            if (entry.permissions!=std::filesystem::perms::unknown)
                ImGui::Text("Permissions: %04o%s",static_cast<unsigned>(entry.permissions)&07777u,entry.executable ? " executable":"");
            if (entry.kind==EntryKind::Directory) {
                const auto& state=fs.state(entry.id);
                ImGui::TextUnformatted(state.loading ? "Loading preview...":state.state==LoadState::Unloaded ? "Preview not loaded":
                    state.state==LoadState::Preview ? "Partial preview":state.state==LoadState::Error ? "Directory unavailable":"Contents loaded");
                if (!state.message.empty()) ImGui::TextWrapped("%s",state.message.c_str());
            }
            if (entry.kind==EntryKind::Symlink) ImGui::TextWrapped("Link -> %s (not traversed)",entry.link_target.c_str());
            if (!entry.error.empty()) ImGui::TextWrapped("%s",entry.error.c_str());
        }
        if (ImGui::Button("Focus (F)")) action={ActionKind::Focus,navigation.selected()};
        ImGui::SameLine();
        if (ImGui::Button("Enter / expand")) action={ActionKind::Open,navigation.selected()};
    }
    ImGui::Checkbox("Animate navigation",&animations);
    ImGui::TextWrapped("Tab: next | Enter: enter/expand\nPgUp/PgDn: page\nRight-drag: orbit | Wheel: zoom\nClick scene: focus | Double-click: enter");
    ImGui::End();
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
        const auto size=ImGui::CalcTextSize(label.text.c_str());
        const ImVec2 pos{left+point->x-size.x/2,point->y-size.y};
        const ImVec4 rect{pos.x-3,pos.y-2,pos.x+size.x+3,pos.y+size.y+2};
        if (rect.x<left || rect.z>left+width || rect.y<0 || rect.w>height) continue;
        bool overlaps=false;
        for (std::size_t i=0;i<count;++i)
            if (rect.x<occupied[i].z && rect.z>occupied[i].x && rect.y<occupied[i].w && rect.w>occupied[i].y)
                overlaps=true;
        if (overlaps) continue;
        if (count<occupied.size()) occupied[count++]=rect;
        draw->AddRectFilled({rect.x,rect.y},{rect.z,rect.w},IM_COL32(4,7,10,220));
        draw->AddText(pos,ImGui::ColorConvertFloat4ToU32({label.color.x,label.color.y,label.color.z,1}),label.text.c_str());
    }
    draw->PopClipRect();
}
}
