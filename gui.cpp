#include "gui.hpp"

#include "imgui.h"
#include "imgui_stdlib.h"

#include <chrono>

namespace backup_gui
{
    void setupGUI(Task & task)
    {
        // Enable docking
        ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());

        setupOptionsWindow(task);
        setupOutputWindow(task);
    }

    void setupOptionsWindow(Task & task)
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

    void setupOutputWindow(Task & task)
    {
        ImGui::Begin("Output");

        //
        ImGui::Text("Directory Comparer");
        ImGui::Indent();
        ImGui::Text("Queued: %d", task.dirStatus.queue_size);

        ImGui::Text(
            "Threads/Busy: %d/%d",
            task.dirStatus.resource_count,
            task.dirStatus.resource_busy_count);

        ImGui::Text("Completed: %d", task.dirStatus.completed_count);
        ImGui::Unindent();

        ImGui::Dummy(ImVec2(0.0f, 20.0f));

        //
        ImGui::Text("File Comparer");
        ImGui::Indent();
        ImGui::Text("Queued: %d", task.fileStatus.queue_size);

        ImGui::Text(
            "Threads/Busy: %d/%d",
            task.fileStatus.resource_count,
            task.fileStatus.resource_busy_count);

        ImGui::Text("Completed: %d", task.fileStatus.completed_count);
        ImGui::Text("Progress: %d", task.fileStatus.progress_sum);
        ImGui::Unindent();

        ImGui::Dummy(ImVec2(0.0f, 20.0f));

        //
        ImGui::Text("File Copier");
        ImGui::Indent();
        ImGui::Text("Queued: %d", task.copyStatus.queue_size);

        ImGui::Text(
            "Threads/Busy: %d/%d",
            task.copyStatus.resource_count,
            task.copyStatus.resource_busy_count);

        ImGui::Text("Completed: %d", task.copyStatus.completed_count);
        ImGui::Text("Progress: %d", task.copyStatus.progress_sum);
        ImGui::Unindent();

        ImGui::Dummy(ImVec2(0.0f, 20.0f));

        //
        ImGui::Text("File Deleter");
        ImGui::Indent();
        ImGui::Text("Queued: %d", task.removeStatus.queue_size);

        ImGui::Text(
            "Threads/Busy: %d/%d",
            task.removeStatus.resource_count,
            task.removeStatus.resource_busy_count);

        ImGui::Text("Completed: %d", task.removeStatus.completed_count);
        ImGui::Text("Progress: %d", task.removeStatus.progress_sum);
        ImGui::Unindent();

        ImGui::Dummy(ImVec2(0.0f, 20.0f));

        ImGui::End();
    }

    void Task::backupLoop()
    {
        while (backupOnce())
            ;
    }

    bool Task::backupOnce()
    {
        // reset to be ready to backup again
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

        // collect options
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

        // backup
        {
            std::scoped_lock lock(mutex);
            m_toolUPtr = std::make_unique<backup::BackupTool>(commandLineArgs);
        }

        m_toolUPtr->run();

        return true;
    }

    void Task::updateLoop()
    {
        while (true)
        {
            {
                std::scoped_lock lock(mutex);

                if (is_quitting)
                {
                    return;
                }

                if (is_running && m_toolUPtr.get())
                {
                    fileStatus   = m_toolUPtr->fileCompareTaskerStatus();
                    dirStatus    = m_toolUPtr->directoryCompareTaskerStatus();
                    copyStatus   = m_toolUPtr->copyTaskerStatus();
                    removeStatus = m_toolUPtr->removeTaskerStatus();
                }
                else
                {
                    fileStatus   = backup::TaskQueueStatus();
                    dirStatus    = backup::TaskQueueStatus();
                    copyStatus   = backup::TaskQueueStatus();
                    removeStatus = backup::TaskQueueStatus();
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }

} // namespace backup_gui
