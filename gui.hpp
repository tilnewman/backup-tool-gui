#include "backup-tool.hpp"

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace backup_gui
{
    enum class Status
    {
        Wait,
        Work,
        Quit
    };

    enum Job : int
    {
        Compare = 0,
        Copy,
        Cull
    };

    struct TaskStatus
    {
        backup::TaskQueueStatus stats;
        std::vector<float> unit_vec;
        std::vector<std::size_t> value_vec;

        void reset();
        void updateVectors();
    };

    struct Task
    {
        // task states
        std::mutex mutex;
        Status status    = Status::Wait;
        bool is_running  = false;
        bool is_quitting = false;
        TaskStatus file_status;
        TaskStatus dir_status;
        TaskStatus copy_status;
        TaskStatus remove_status;

        // job states
        int job = Job::Compare;
        std::string src_dir;
        std::string dst_dir;
        bool opt_dryrun          = false;
        bool opt_background      = false;
        bool opt_skipread        = false;
        bool opt_relative        = false;
        bool opt_verbose         = false;
        bool opt_ignore_extra    = false;
        bool opt_ignore_access   = false;
        bool opt_ignore_unknown  = false;
        bool opt_ignore_warnings = false;

        // job workers
        std::unique_ptr<backup::BackupTool> m_toolUPtr;

        void backupLoop();
        bool backupOnce();

        void updateLoop();
    };

    void setupGUI(Task & task);
    void setupOptionsWindow(Task & task);
    void setupOutputWindow(Task & task);
    void setupStatusBlock(Task & task, const std::string & title, const TaskStatus & status);
} // namespace backup_gui
