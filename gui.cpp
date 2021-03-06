#include "gui.hpp"

#include "imgui.h"
#include "imgui_stdlib.h"

#include <algorithm>
#include <chrono>

namespace backup_gui
{

    static void HelpMarker(const char * desc)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(desc);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    void setupGUI(Task & task)
    {
        ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());
        setupOptionsWindow(task);
        setupOutputWindow(task);
    }

    void setupOptionsWindow(Task & task)
    {
        ImGui::Begin("Options");

        if (task.is_running)
        {
            ImGui::BeginDisabled();
        }

        ImGui::RadioButton("Compare", &task.job, Job::Compare);
        ImGui::SameLine();
        ImGui::RadioButton("Copy", &task.job, Job::Copy);
        ImGui::SameLine();
        ImGui::RadioButton("Cull", &task.job, Job::Cull);

        ImGui::InputText("Source", &task.src_dir);
        ImGui::InputText("Destination", &task.dst_dir);

        ImGui::Checkbox("Dry Run", &task.opt_dryrun);
        HelpMarker("A safe mode that does nothing except show what WOULD have been done");

        ImGui::Checkbox("Single Thread", &task.opt_background);
        HelpMarker("Runs minimal threads to prevent slowing your computer down");

        ImGui::Checkbox("Skip File Content Compare", &task.opt_skipread);
        HelpMarker("Files with the exact same size are assumed to have the same contents");

        ImGui::Checkbox("Relative Paths", &task.opt_relative);
        HelpMarker("Displays relative paths instead of absolute paths");

        ImGui::Checkbox("Verbose", &task.opt_verbose);
        HelpMarker("Shows extra info. (i.e. warns on symlinks/shortcuts/weird stuff)");

        ImGui::Checkbox("Ignore Extra", &task.opt_ignore_extra);
        HelpMarker("Any extra files or dirs in your dst dir are not shown");

        ImGui::Checkbox("Ignore Access", &task.opt_ignore_access);
        HelpMarker("Errors caused by access/permissions/authentication problems are not shown");

        ImGui::Checkbox("Ignore Unknown", &task.opt_ignore_unknown);
        HelpMarker("Errors caused by files or dirs with unknown types are not shown");

        ImGui::Checkbox("Ignore Warnings", &task.opt_ignore_warnings);
        HelpMarker("Warnings about unusual counts or possible errors are not shown");

        const bool wasButtonClicked = ImGui::Button("Execute", ImVec2(600.0f, 50.0f));

        if (task.is_running)
        {
            ImGui::EndDisabled();
        }

        if (wasButtonClicked && !task.is_running)
        {
            task.is_running = true;
            task.cond_var.notify_all();
        }

        ImGui::Text("Status:  ");
        ImGui::SameLine();

        if (task.status == Status::Quitting)
        {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Quitting");
        }
        else if (task.status == Status::Working)
        {
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "Working");
        }
        else
        {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "Ready");
        }

        ImGui::End();
    }

    void setupOutputWindow(Task & task)
    {
        std::scoped_lock lock(task.mutex);
        ImGui::Begin("Output");
        setupStatusBlock("Directory Comparer", task.dir_status);
        setupStatusBlock("File Comparer", task.file_status);
        setupStatusBlock("File Copier", task.copy_status);
        setupStatusBlock("File Deleter", task.remove_status);
        ImGui::End();
    }

    void setupStatusBlock(const std::string & title, const TaskStatus & status)
    {
        ImGui::Text("%s", title.data(), title.size());
        ImGui::Indent();

        ImGui::Text(
            "Threads/Busy: %d/%d", status.stats.resource_count, status.stats.resource_busy_count);

        ImGui::Text(
            "Queued/Completed: %d/%d", status.stats.queue_size, status.stats.completed_count);

        ImGui::PlotLines(
            "",
            &status.unit_vec[0],
            static_cast<int>(status.unit_vec.size()),
            0,
            NULL,
            0.0f,
            1.0f,
            ImVec2(600.0f, 80.0f));

        ImGui::Unindent();
        ImGui::Dummy(ImVec2(0.0f, 20.0f));
    }

    void Task::backupLoop()
    {
        while (!is_quitting)
        {
            backupOnce();
        }
    }

    void Task::backupOnce()
    {
        std::unique_lock lock(mutex);

        // reset states to be ready to backup again
        status      = Status::Waitting;
        is_running  = false;
        is_quitting = false;

        cond_var.wait(lock, [&] { return (is_running || is_quitting); });

        if (is_quitting)
        {
            status = Status::Quitting;
            return; // unique_lock will unlock() here upon leaving scope
        }

        if (is_running)
        {
            status = Status::Working;
        }

        // reset all status to be ready to backup again
        file_status.reset();
        dir_status.reset();
        copy_status.reset();
        remove_status.reset();

        // collect options & backup
        std::vector<std::string> commandLineArgs;

        if (Job::Copy == job)
        {
            commandLineArgs.push_back("--copy");
        }
        else if (Job::Cull == job)
        {
            commandLineArgs.push_back("--cull");
        }
        else
        {
            commandLineArgs.push_back("--compare");
        }

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
            commandLineArgs.push_back("--show-relative");
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

        commandLineArgs.push_back(src_dir);
        commandLineArgs.push_back(dst_dir);

        m_toolUPtr = std::make_unique<backup::BackupTool>(commandLineArgs);

        // don't hold the lock while the backup job is running
        lock.unlock();

        m_toolUPtr->run();
    }

    void Task::updateLoop()
    {
        while (!is_quitting)
        {
            updateOnce();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    void Task::updateOnce()
    {
        std::scoped_lock lock(mutex);

        if (is_running && m_toolUPtr.get())
        {
            file_status.stats   = m_toolUPtr->fileCompareTaskerStatus();
            dir_status.stats    = m_toolUPtr->directoryCompareTaskerStatus();
            copy_status.stats   = m_toolUPtr->copyTaskerStatus();
            remove_status.stats = m_toolUPtr->removeTaskerStatus();

            file_status.updateVectors();
            dir_status.updateVectors();
            copy_status.updateVectors();
            remove_status.updateVectors();
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
