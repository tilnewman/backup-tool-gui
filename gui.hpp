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

    struct Task
    {
        // task states
        std::mutex mutex;
        Status status    = Status::Wait;
        bool is_running  = false;
        bool is_quitting = false;

        // job states
        int job = Job::Compare;
        std::string srcDir;
        std::string dstDir;
        bool opt_dryrun          = false;
        bool opt_background      = false;
        bool opt_skipread        = false;
        bool opt_relative        = false;
        bool opt_verbose         = false;
        bool opt_ignore_extra    = false;
        bool opt_ignore_access   = false;
        bool opt_ignore_unknown  = false;
        bool opt_ignore_warnings = false;

        std::vector<std::string> output;

        void work();
        bool workOnce();
    };

    void setupGUI(Task & task);

} // namespace backup_gui
