#include "backup-tool.hpp"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace backup_gui
{
    enum class Status
    {
        Waitting,
        Working,
        Quitting
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
        std::condition_variable cond_var;
        Status status    = Status::Waitting;
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
        void backupOnce();

        void updateLoop();
        void updateOnce();
    };

    void setupGUI(Task & task);
    void setupOptionsWindow(Task & task);
    void setupOutputWindow(Task & task);
    void setupStatusBlock(const std::string & title, const TaskStatus & status);
} // namespace backup_gui
