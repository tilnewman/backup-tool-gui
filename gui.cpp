#include "gui.hpp"

#include "imgui.h"

namespace backup_gui
{
    void setupGUI(State & state)
    {

        // Enable docking
        ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());

        {
            ImGui::Begin("Options");

            if (state.is_running)
            {
                ImGui::BeginDisabled();
            }

            ImGui::RadioButton("Compare", &state.job, Job::Compare);
            ImGui::SameLine();
            ImGui::RadioButton("Copy", &state.job, Job::Copy);
            ImGui::SameLine();
            ImGui::RadioButton("Cull", &state.job, Job::Cull);

            ImGui::Text("(%.1f FPS)", ImGui::GetIO().Framerate);

            if (state.is_running)
            {
                ImGui::EndDisabled();
            }

            ImGui::End();
        }

        {
            ImGui::Begin("Output");
            ImGui::End();
        }
    }
} // namespace backup_gui
