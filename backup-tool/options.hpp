#ifndef BACKUP_OPTIONS_HPP_INCLUDED
#define BACKUP_OPTIONS_HPP_INCLUDED
//
// options.hpp
//
#include "entry.hpp"
#include "enums.hpp"
#include "filesystem-common.hpp"

#include <string>
#include <vector>

namespace backup
{

    struct ThreadCounts
    {
        std::size_t total_detected = 0;
        std::size_t dir_compare    = 0;
        std::size_t file_compare   = 0;
        std::size_t copy           = 0;
        std::size_t remove         = 0;
    };

    struct Options
    {
        Job job = Job::Compare;

        bool dry_run             = false;
        bool background          = false;
        bool verbose             = false;
        bool quiet               = false;
        bool skip_file_read      = false;
        bool ignore_access_error = false;
        bool ignore_extra        = false;
        bool ignore_unknown      = false;
        bool ignore_warnings     = false;
        bool show_relative_path  = false;

        ThreadCounts thread_counts;

        DirPair<fs::path> path_dpair;
        DirPair<std::wstring> path_str_dpair;
        DirPair<Entry> entry_dpair;

        // disable color by default on windows because it rarely ever works
        static bool isColorEnabledByDefault() { return !is_running_on_windows; }
    };

} // namespace backup

#endif // BACKUP_OPTIONS_HPP_INCLUDED
