#include "ui/overlay.hpp"
#include <imgui.h>

namespace spatial {
void overlay(Navigation& navigation,Camera& camera,bool& animations) {
    ImGui::SetNextWindowPos({16,16},ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.92f);
    ImGui::Begin("Spatial navigator",nullptr,ImGuiWindowFlags_AlwaysAutoResize|
        ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoCollapse);
    ImGui::TextUnformatted("MILESTONE 1 / MOCK SCENE");
    ImGui::Text("Focus: %s",navigation.directory().path.data());
    ImGui::TextUnformatted(camera.moving() ? "Camera: moving" : "Camera: ready");
    ImGui::Separator();
    for (std::size_t i=0;i<directories.size();++i) {
        if (ImGui::Button(directories[i].path.data()))
            camera.focus(navigation.focus(i),animations);
        if (i==0) ImGui::SameLine();
    }
    if (ImGui::Button("Reset view (R)")) camera.reset(animations);
    ImGui::Checkbox("Animate navigation",&animations);
    ImGui::TextUnformatted("Right-drag: orbit | Wheel: zoom");
    ImGui::TextUnformatted("Tab: focus next directory | Esc: quit");
    ImGui::End();
}
}
