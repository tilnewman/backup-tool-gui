#include "gui.hpp"

#include "imgui.h"
#include "imgui_stdlib.h"

#include <algorithm>
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

        ImGui::InputText("SourceDir", &task.src_dir);
        ImGui::InputText("DestDir", &task.dst_dir);

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

        ImGui::Text(
            "Queued/Completed: %d/%d",
            task.dir_status.stats.queue_size,
            task.dir_status.stats.completed_count);

        ImGui::Text(
            "Threads/Busy: %d/%d",
            task.dir_status.stats.resource_count,
            task.dir_status.stats.resource_busy_count);

        ImGui::PlotLines(
            "",
            &task.dir_status.unit_vec[0],
            static_cast<int>(task.dir_status.unit_vec.size()),
            0,
            NULL,
            0.0f,
            1.0f,
            ImVec2(600.0f, 80.0f));

        ImGui::Unindent();
        ImGui::Dummy(ImVec2(0.0f, 20.0f));

        //
        ImGui::Text("File Comparer");
        ImGui::Indent();

        ImGui::Text(
            "Queued/Completed: %d/%d",
            task.file_status.stats.queue_size,
            task.file_status.stats.completed_count);

        ImGui::Text(
            "Threads/Busy: %d/%d",
            task.file_status.stats.resource_count,
            task.file_status.stats.resource_busy_count);

        ImGui::PlotLines(
            "",
            &task.file_status.unit_vec[0],
            static_cast<int>(task.file_status.unit_vec.size()),
            0,
            NULL,
            0.0f,
            1.0f,
            ImVec2(600.0f, 80.0f));

        // ImGui::Text("Progress: %d", task.file_status.stats.progress_sum);
        ImGui::Unindent();
        ImGui::Dummy(ImVec2(0.0f, 20.0f));

        //
        ImGui::Text("File Copier");
        ImGui::Indent();

        ImGui::Text(
            "Queued/Completed: %d/%d",
            task.copy_status.stats.queue_size,
            task.copy_status.stats.completed_count);

        ImGui::Text(
            "Threads/Busy: %d/%d",
            task.copy_status.stats.resource_count,
            task.copy_status.stats.resource_busy_count);

        ImGui::PlotLines(
            "",
            &task.copy_status.unit_vec[0],
            static_cast<int>(task.copy_status.unit_vec.size()),
            0,
            NULL,
            0.0f,
            1.0f,
            ImVec2(600.0f, 80.0f));

        // ImGui::Text("Progress: %d", task.copy_status.stats.progress_sum);
        ImGui::Unindent();
        ImGui::Dummy(ImVec2(0.0f, 20.0f));

        //
        ImGui::Text("File Deleter");
        ImGui::Indent();

        ImGui::Text(
            "Queued/Completed: %d/%d",
            task.remove_status.stats.queue_size,
            task.remove_status.stats.completed_count);

        ImGui::Text(
            "Threads/Busy: %d/%d",
            task.remove_status.stats.resource_count,
            task.remove_status.stats.resource_busy_count);

        ImGui::PlotLines(
            "",
            &task.remove_status.unit_vec[0],
            static_cast<int>(task.remove_status.unit_vec.size()),
            0,
            NULL,
            0.0f,
            1.0f,
            ImVec2(600.0f, 80.0f));

        // ImGui::Text("Progress: %d", task.remove_status.stats.progress_sum);
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
                    file_status.reset();
                    dir_status.reset();
                    copy_status.reset();
                    remove_status.reset();
                    break;
                }
            }

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // collect options & backup
        std::vector<std::string> commandLineArgs;
        {
            std::scoped_lock lock(mutex);
            commandLineArgs.push_back(src_dir);
            commandLineArgs.push_back(dst_dir);

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
                    file_status.stats   = m_toolUPtr->fileCompareTaskerStatus();
                    dir_status.stats    = m_toolUPtr->directoryCompareTaskerStatus();
                    copy_status.stats   = m_toolUPtr->copyTaskerStatus();
                    remove_status.stats = m_toolUPtr->removeTaskerStatus();
                }
                else
                {
                    file_status.stats   = backup::TaskQueueStatus();
                    dir_status.stats    = backup::TaskQueueStatus();
                    copy_status.stats   = backup::TaskQueueStatus();
                    remove_status.stats = backup::TaskQueueStatus();
                }

                file_status.updateVectors();
                dir_status.updateVectors();
                copy_status.updateVectors();
                remove_status.updateVectors();
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }

    void TaskStatus::reset()
    {
        stats = backup::TaskQueueStatus();
        unit_vec.clear();
        value_vec.clear();
    }

    void TaskStatus::updateVectors()
    {
        if (stats.isDone())
        {
            return;
        }

        unit_vec.clear();

        value_vec.push_back(stats.queue_size);

        const std::size_t max = *std::max_element(std::begin(value_vec), std::end(value_vec));

        for (const std::size_t value : value_vec)
        {
            unit_vec.push_back(static_cast<float>(value) / static_cast<float>(max));
        }
    }

} // namespace backup_gui
