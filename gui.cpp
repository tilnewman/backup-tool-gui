#include "gui.hpp"

#include "backup-tool.hpp"

#include "imgui.h"
#include "imgui_stdlib.h"

#include <chrono>

namespace backup_gui
{
    void setupGUI(Task & task)
    {

        // Enable docking
        ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());

        {
            ImGui::Begin("Options");

            {
                std::scoped_lock lock(task.mutex);
                if (task.is_running)
                {
                    ImGui::BeginDisabled();
                }
            }

            ImGui::RadioButton("Compare", &task.job, Job::Compare);
            ImGui::SameLine();
            ImGui::RadioButton("Copy", &task.job, Job::Copy);
            ImGui::SameLine();
            ImGui::RadioButton("Cull", &task.job, Job::Cull);

            ImGui::InputText("SourceDir", &task.srcDir);
            ImGui::InputText("DestDir", &task.dstDir);

            ImGui::Checkbox("Dry Run", &task.opt_dryrun);
            ImGui::Checkbox("Single Thread", &task.opt_background);
            ImGui::Checkbox("Skip Read", &task.opt_skipread);
            ImGui::Checkbox("Relative", &task.opt_relative);
            ImGui::Checkbox("Verbose", &task.opt_verbose);
            ImGui::Checkbox("Ignore Extra", &task.opt_ignore_extra);
            ImGui::Checkbox("Ignore Access", &task.opt_ignore_access);
            ImGui::Checkbox("Ignore Unknown", &task.opt_ignore_unknown);
            ImGui::Checkbox("Ignore Warnings", &task.opt_ignore_warnings);

            bool wasButtonClicked = false;
            {
                std::scoped_lock lock(task.mutex);
                wasButtonClicked = ImGui::Button("Execute");

                if (task.is_running)
                {
                    ImGui::EndDisabled();
                }

                if (wasButtonClicked && !task.is_running)
                {
                    task.is_running = true;
                }
            }

            {
                std::scoped_lock lock(task.mutex);
                ImGui::Text("Status:  ");
                ImGui::SameLine();

                if (task.status == Status::Quit)
                {
                    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Quitting");
                }
                else if (task.status == Status::Work)
                {
                    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Working");
                }
                else
                {
                    ImGui::TextColored(ImVec4(1, 1, 1, 1), "Waiting");
                }
            }

            ImGui::End();
        }

        {
            ImGui::Begin("Output");

            std::scoped_lock lock(task.mutex);
            for (const std::string & str : task.output)
            {
                ImGui::Text("%s", str.data(), str.size());
            }

            ImGui::End();
        }
    }

    void Task::work()
    {
        while (workOnce())
            ;
    }

    bool Task::workOnce()
    {
        // reset to be ready to work again
        {
            std::scoped_lock lock(mutex);
            status      = Status::Wait;
            is_running  = false;
            is_quitting = false;
        }

        // wait for signal to start
        while (true)
        {
            {
                std::scoped_lock lock(mutex);
                if (is_quitting)
                {
                    status = Status::Quit;
                    return false;
                }
                else if (is_running)
                {
                    status = Status::Work;
                    break;
                }
            }

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // do work
        std::vector<std::string> commandLineArgs;
        {
            std::scoped_lock lock(mutex);
            commandLineArgs.push_back(srcDir);
            commandLineArgs.push_back(dstDir);

            if (opt_dryrun)
            {
                commandLineArgs.push_back("--dry-run");
            }

            if (opt_background)
            {
                commandLineArgs.push_back("--background");
            }

            if (opt_skipread)
            {
                commandLineArgs.push_back("--skip-file-read");
            }

            if (opt_relative)
            {
                commandLineArgs.push_back("--relative");
            }

            if (opt_verbose)
            {
                commandLineArgs.push_back("--verbose");
            }

            if (opt_ignore_extra)
            {
                commandLineArgs.push_back("--ignore-extra");
            }

            if (opt_ignore_access)
            {
                commandLineArgs.push_back("--ignore-access");
            }

            if (opt_ignore_unknown)
            {
                commandLineArgs.push_back("--ignore-unknown");
            }

            if (opt_ignore_warnings)
            {
                commandLineArgs.push_back("--ignore-warnings");
            }
        }

        backup::BackupTool tool(commandLineArgs);
        tool.run();

        return true;
    }

} // namespace backup_gui
